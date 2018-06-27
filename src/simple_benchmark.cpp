#include <chrono>
#include <iostream>

#include <irkit/coding/varbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/posting_list.hpp>
#include <irkit/io.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/index/source.hpp>

int main(int argc, char** argv)
{
    assert(argc == 2);
    irk::fs::path index_dir(argv[1]);
    std::ifstream term_in(irk::index::term_map_path(index_dir).c_str());
    std::ifstream title_in(irk::index::title_map_path(index_dir).c_str());
    irk::inverted_index_mapped_data_source data(index_dir);
    irk::inverted_index_view index_view(&data, irk::varbyte_codec<long>{},
                                            irk::varbyte_codec<long>{},
                                            irk::varbyte_codec<long>{});

    std::chrono::nanoseconds elapsed(0);
    long posting_count;

    for (long term_id = 0; term_id < index_view.terms().size(); term_id++) {
        auto postings = index_view.postings(term_id);
        std::vector<std::pair<long, long>> vec(postings.size());
        auto start_interval = std::chrono::steady_clock::now();
        std::copy(postings.begin(), postings.end(), vec.begin());
        auto end_interval = std::chrono::steady_clock::now();
        elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_interval - start_interval);
        posting_count += vec.size();
        assert(postings.size() == vec.size());
    }

    long time =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    double avg_time = (double) time / index_view.terms().size();
    double time_per_posting = (double) time / posting_count;
    std::cout << "Avg traversal time: " << avg_time << "ms (" << time_per_posting
              << "ms/posting)" << std::endl;
    return 0;
}
