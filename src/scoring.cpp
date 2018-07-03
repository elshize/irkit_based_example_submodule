#include <chrono>
#include <iostream>

#include <irkit/coding/varbyte.hpp>
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

    irk::inverted_index_mapped_data_source data(
        index_dir, irk::score::bm25_tag{});
    irk::inverted_index_view index_view(&data, irk::varbyte_codec<irk::index::document_t>{},
                                            irk::varbyte_codec<long>{},
                                            irk::varbyte_codec<long>{});

    for (const auto& posting : index_view.scored_postings(term_id)) {
        std::cout << posting.document() << "\t"
                  << posting.payload() << std::endl;
    }
    return 0;
}
