#include <chrono>
#include <iostream>

#include <irkit/index.hpp>
#include <irkit/index/posting_list.hpp>
#include <irkit/io.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/score.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>

int main(int argc, char** argv)
{
    assert(argc == 3);
    irk::fs::path index_dir(argv[1]);
    long term_id = std::stol(argv[2]);

    irk::inverted_index_mapped_data_source data(index_dir, "bm25");
    irk::inverted_index_view index_view(&data);

    for (const auto& posting : index_view.scored_postings(term_id)) {
        std::cout << posting.document() << "\t"
                  << posting.payload() << std::endl;
    }
    return 0;
}
