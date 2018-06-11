#include <iostream>

#include <irkit/index/vector_inverted_list.hpp>
#include <irkit/index/posting_list.hpp>

template<class PostingT>
void print_posting(PostingT posting)
{
    std::cout << posting.document() << "\t"
              << posting.payload() << std::endl;
}

template<class PostingListT>
void traverse_list(PostingListT posting_list)
{
    for (const auto& posting : posting_list) { print_posting(posting); }
}

/*!
 * This function traverses postings until score is
 * at least 4.0, and then skips to ID 17, and
 * continues traversal till the end of the list.
 */
template<class PostingListT>
void peculiar_function(PostingListT posting_list)
{
    auto it = posting_list.begin();
    for (; it->payload() < 4.0; it++) { print_posting(*it); }
    it.moveto(17);
    for (; it != posting_list.end(); it++) { print_posting(*it); }
}

int main(int argv, char** args)
{
    irk::vector_document_list docs({
            2, 4, 5, 9, 10, 17, 29});
    irk::vector_payload_list<double> scores({
            1.0, 2.0, 4.0, 3.0, 0.6, 6.0, 77.0});
    docs.block_size(3);
    scores.block_size(3);
    irk::posting_list_view postings(docs, scores);
    std::cout << "Traversing all postings:\nID\tScore\n";
    traverse_list(postings);
    std::cout << "With peculiar:\nID\tScore\n";
    peculiar_function(postings);
    return 0;
}
