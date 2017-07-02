#include "common.h"
#include "mimir.h"

using namespace MIMIR_NS;

#define WORD_LEN_MEAN_DEFAULT 6
#define WORD_LEN_SD_DEFAULT   1

void parse_cmd_line(int argc, char **argv);

void map_nums (Readable<double, void> *input,
               Writable<double, void> *output, void *ptr);
void map_words (Readable<double, void> *input,
                Writable<const char*, void> *output, void *ptr);
void map_copy (Readable<const char*, void> *input,
               Writable<const char*, void> *output, void *ptr);

void map_uniques (Readable<const char*,void> *input,
                  Writable<const char*,void> *output, void *ptr);

void combine (Combinable<const char*,void> *combiner,
              const char**, void*, void*, void*, void *ptr);
void scanedge (const char ** word, void *val, void* ptr);

const char *cmdstr = "./cmd \t<itemcount> <outfile>\n\
\t--zipf-n [val]\n\
\t--zipf-alpha [val]\n\
\t--stat-file [val]\n\
\t-disorder\n\
\t-single-file\n";

uint64_t itemcount = 0;
const char *outfile = NULL;
//const char *dictfile = 0;
uint64_t zipf_n = 0;
double zipf_alpha = 0.0;
const char *statfile = NULL;
bool disorder = false;
bool singlefile = false;

uint64_t total_unique = 0;
uint64_t remain_unique = 0;
int len_mean = WORD_LEN_MEAN_DEFAULT;
double len_std = WORD_LEN_SD_DEFAULT;

int proc_rank, proc_size;
double    *dist_map = NULL;
double    *dist_new_map = NULL;
uint64_t  *div_idx_map = NULL;
double    *div_dist_map = NULL;
uint64_t  *word_counts = NULL;
uint64_t  *div_map = NULL;

std::vector<std::string> unique_words;
std::vector<std::string> unique_new_words;

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> *dis = NULL;

int parititon_num (double *num, void *null, int npartition) {

    for (int i = 1; i < proc_size + 1; i ++) {
        if (div_dist_map[i] >= *num) return i - 1;
    }

    return proc_size - 1;
}

int partition_word (const char **word, void* null, int npartition) {

    return (*dis)(gen);
}

double t_copy = 0.0, t_map = 0.0, t_cvt = 0.0;

int main(int argc, char **argv) {

    parse_cmd_line(argc, argv);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &proc_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &proc_size);

    dis = new std::uniform_int_distribution<>(0, proc_size - 1);

    double t1 = MPI_Wtime();

    total_unique = zipf_n;
    uint64_t div_size = zipf_n / proc_size;
    if (proc_rank < (zipf_n % proc_size)) div_size += 1;
    dist_map = new double[div_size];
    div_idx_map = new uint64_t[proc_size + 1];
    div_dist_map = new double[proc_size + 1];

    gen_dist_map(zipf_n, zipf_alpha, dist_map, div_idx_map, div_dist_map);

    double t2 = MPI_Wtime();
    if (proc_rank == 0) {
        fprintf(stdout, "generate distribution map (walltime=%lf)\n", t2 - t1);
    }

    div_size = div_idx_map[proc_rank + 1] - div_idx_map[proc_rank];
    dist_new_map = new double[div_size+1];
    word_counts = new uint64_t[div_size];

    for (uint64_t i = 0; i < div_size; i++) {
        word_counts[i] = 0;
    }

    repartition_dist_map(zipf_n, dist_map, div_idx_map, dist_new_map);

    double t3 = MPI_Wtime();
    if (proc_rank == 0) {
        fprintf(stdout, "repartition distribution map (walltime=%lf)\n", t3 - t2);
    }

    std::string output;
    std::vector<std::string> input;

    MIMIR_NS::MimirContext<const char*, void>* unique_words_ctx 
        = new MIMIR_NS::MimirContext<const char*, void>(MPI_COMM_WORLD, map_uniques, NULL,
                                        input, output, NULL, combine);

    // Generate remain_unique words
    remain_unique = total_unique;
    while (1) {
        uint64_t nunique = unique_words_ctx->map();
        remain_unique = total_unique - nunique;
        if (proc_rank == 0) {
            fprintf(stdout, "generate %ld unique words\n",
                    total_unique - remain_unique);
        }
        if (remain_unique == 0) break;
    }

    double t4 = MPI_Wtime();
    if (proc_rank == 0) {
        fprintf(stdout, "generate unique words (walltime=%lf)\n", t4 - t3);
    }

    MIMIR_NS::MimirContext<const char*, void>* ctx_ptr = NULL;

    if (disorder) {
        MIMIR_NS::MimirContext<const char*, void>* disorder_words_ctx 
            = new MIMIR_NS::MimirContext<const char*, void>(MPI_COMM_WORLD, map_copy, NULL,
                                            input, output, NULL, NULL, partition_word);
        disorder_words_ctx->insert_data(unique_words_ctx->get_output_handle());
        disorder_words_ctx->map();

        delete unique_words_ctx;

        ctx_ptr = disorder_words_ctx;

    } else {
        ctx_ptr = unique_words_ctx;
    }

    double t5 = MPI_Wtime();
    if (proc_rank == 0) {
        fprintf(stdout, "disorder unique words (walltime=%lf)\n", t5 - t4);
    }

    unique_words.clear();
    ctx_ptr->scan(scanedge);

    repartition_unique_words(unique_words, unique_new_words, div_idx_map);

    double t6 = MPI_Wtime();
    if (proc_rank == 0) {
        fprintf(stdout, "repartition unique words (walltime=%lf)\n", t6 - t5);
    }

    MIMIR_NS::MimirContext<double, void>* num_ctx 
        = new MIMIR_NS::MimirContext<double, void>(MPI_COMM_WORLD, map_nums, NULL,
                                                   input, output, NULL, NULL, parititon_num);
    delete ctx_ptr;

    if (proc_rank == 0) {
        fprintf(stdout, "start generate numbers\n");
    }

    num_ctx->map();

    double t7 = MPI_Wtime();
    if (proc_rank == 0) {
        fprintf(stdout, "generate numbers (walltime=%lf)\n", t7 - t6);
    }

    MIMIR_NS::MimirContext<const char*, void, double, void>* word_ctx 
        = new MIMIR_NS::MimirContext<const char*, void, double, void>(
                MPI_COMM_WORLD, map_words, NULL, input, output, 
                NULL, NULL, partition_word);
    word_ctx->set_outfile_format("text");
    word_ctx->insert_data(num_ctx->get_output_handle());

    word_ctx->map();

    double t8 = MPI_Wtime();
    if (proc_rank == 0) {
        fprintf(stdout, "generate words (walltime=%lf)\n", t8 - t7);
    }

    delete num_ctx;

    output = outfile;
    MIMIR_NS::MimirContext<const char*, void>* partition_ctx 
        = new MIMIR_NS::MimirContext<const char*, void>(
                MPI_COMM_WORLD, map_copy, NULL, input, output, 
                NULL, NULL, partition_word, true, IMPLICIT_OUTPUT);
    partition_ctx->set_outfile_format("text");
    partition_ctx->insert_data(word_ctx->get_output_handle());

    partition_ctx->map();

    double t9 = MPI_Wtime();
    if (proc_rank == 0) {
        fprintf(stdout, "disorder words (walltime=%lf)\n", t9 - t8);
    }

    delete word_ctx;
    delete partition_ctx;

    delete [] dist_new_map;
    delete [] div_dist_map;
    delete [] div_idx_map;
    delete [] dist_map;

    if (statfile != NULL) {
        char filename[1024];
        sprintf(filename, "%s%d.%d", statfile, proc_size, proc_rank);
        std::ofstream outfile;
        outfile.open(filename);
        for (uint64_t i = 0; i < div_size; i++) {
            outfile << unique_new_words[i] << "," << word_counts[i] << std::endl;
        }
        outfile.close();
    }

    delete [] word_counts;

    if (proc_rank == 0) {
        fprintf(stdout, "t_copy = %lf, t_map = %lf, t_cvt = %lf\n",
                t_copy, t_map, t_cvt);
        fflush(stdout);
    }

    delete dis;

    MPI_Finalize();

    return 0;
}

void map_nums (Readable<double,void> *input, 
               Writable<double,void> *output, void *ptr) {
    uint64_t local_items = itemcount / proc_size;
    if (proc_rank < itemcount % proc_size) local_items += 1;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0, 1);
    while (local_items > 0) {
        double num = dis(gen);
        output->write(&num, NULL);
        local_items --;
    }
}

void map_words (Readable<double, void> *input, 
                Writable<const char*, void> *output, void *ptr) {
    double num;
    uint64_t idx;
    const char *word;
    uint64_t range_size = div_idx_map[proc_rank + 1] - div_idx_map[proc_rank];

    while (input->read(&num, NULL) == 0) {

        uint64_t low = 0;
        uint64_t high = range_size + 1;
        uint64_t mid = 0;

        double t1 = MPI_Wtime();

        while (low + 1 < high) {
            mid = (low + high) / 2;
            if (num >= dist_new_map[mid] && num < dist_new_map[mid+1])
                break;
            if (num < dist_new_map[mid] && num >= dist_new_map[mid-1]) {
                mid -= 1;
                break;
            }
            if (num < dist_new_map[mid]) high = mid;
            else low = mid;
        }

        double t2 = MPI_Wtime();
        t_cvt += (t2 - t1);

        idx = mid;
        word = unique_new_words[idx].c_str();
        word_counts[idx] ++;
        output->write(&word, NULL);

    }
}

void map_copy (Readable<const char*, void> *input,
               Writable<const char*, void> *output, void *ptr)
{
    double t1 = MPI_Wtime();
    const char* word = NULL;
    while (input->read(&word, NULL) == 0) {
        output->write(&word, NULL);
    }
    double t2 = MPI_Wtime();
    t_copy += (t2 - t1);
}

void map_uniques (Readable<const char*,void> *input, 
          Writable<const char*,void> *output, void *ptr)
{
    unique_words.clear();

    uint64_t local_unique = remain_unique / proc_size;
    if (proc_rank < (remain_unique % proc_size)) local_unique += 1;

    // Get existing words
    if (input != NULL) {
        const char *exist_word = NULL;
        while (input->read(&exist_word, NULL) == 0) {
            //unique_words.push_back(std::string(exist_word));
            output->write(&exist_word, NULL);
        }
    }

    printf("%d start generate unique words\n", proc_rank);

    // Generate more words
    if (local_unique > 1000000) local_unique = 1000000;
    generate_unique_words(local_unique, unique_words, len_mean, len_std);

    printf("%d end generate unique words\n", proc_rank);

    // Add words into Mimir
    const char *word = NULL;
    for (auto iter : unique_words) {
        word = iter.c_str();
        output->write(&word, NULL);
    }
}

void combine (Combinable<const char*,void> *combiner,
              const char **key, void *val1, void *val2,
              void *rval, void *ptr)
{
    return;
}

void scanedge (const char ** word, void *val, void* ptr) {
    unique_words.push_back(*word);
}

void parse_cmd_line(int argc, char **argv) {

    if (argc < 4) { printf("%s", cmdstr); exit(1); }

    --argc;
    ++argv;
    assert(argc);
    if (**argv == '-') { printf("%s", cmdstr); exit(1); }
    itemcount = atoll(*argv);

    --argc;
    ++argv;
    assert(argc);
    if (**argv == '-') { printf("%s", cmdstr); exit(1); }
    outfile = *argv;

    //--argc;
    //++argv;
    //assert(argc);
    //if (**argv == '-') { printf("%s", cmdstr); exit(1); }
    //dictfile = *argv;

    while (--argc && ++argv) {
        if (!strcmp(*argv, "--zipf-n")) {
            --argc;
            ++argv;
            assert(argc);

            zipf_n = atoll(*argv);
        }
        else if (!strcmp(*argv, "--zipf-alpha")) {
            --argc;
            ++argv;
            assert(argc);

            zipf_alpha = atof(*argv);
        }
        else if (!strcmp(*argv, "--stat-file")) {
            --argc;
            ++argv;
            assert(argc);

            statfile = *argv;
        }
        else if (!strcmp(*argv, "-disorder")) {
            singlefile = true;
        }
        else if (!strcmp(*argv, "-singlefile")) {
            singlefile = true;
        }
        else {
            printf("%s", cmdstr);
        }
    }
}
