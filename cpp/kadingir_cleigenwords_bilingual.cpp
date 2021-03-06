#include "docopt.cpp/docopt.h"
#include "../src/kadingir_core.hpp"
#include "../src/utils.hpp"


static const char USAGE[] =
R"(Kadingir: Cross-Lingual Eigenwords (bilingual)

    Usage:
      kadingir_cleigenwords_bilingual --corpus1 <corpus1> --corpus2 <corpus2> --output <output> --vocab1 <vocab1> --vocab2 <vocab2> --window1 <window1> --window2 <window2> --dim <dim> [--debug]

    Options:
      -h --help            Show this screen.
      --version            Show version.
      --corpus1=<corpus1>  File path of corpus of language 1
      --corpus2=<corpus2>  File path of corpus of language 2
      --output=<output>    File path of output
      --vocab1=<vocab1>    Size of vocabulary of language 1
      --vocab2=<vocab2>    Size of vocabulary of language 2
      --window1=<window1>  Window size of language 1
      --window2=<window2>  Window size of language 2
      --dim=<dim>          Dimension of representation
      --debug              Debug option [default: false]
)";


void set_tokens (std::string path_corpus,
                 const int n_vocab,
                 const int window,
                 std::vector<int> &tokens,
                 std::vector<int> &document_id,
                 std::vector<PairCounter> &count_vector)
{
  // Build word count table
  MapCounter count_table;
  unsigned long long n_tokens = 0;
  unsigned long long n_documents = 0;

  build_count_table(path_corpus, count_table, n_documents, n_tokens);
  
  // Sort `count_table`.
  count_vector = std::vector<PairCounter>(count_table.begin(), count_table.end());
  std::sort(count_vector.begin(), count_vector.end(), sort_greater);
  
  // Construct table (std::string)word -> (int)wordtype id
  unsigned long long i_vocab = 1;
  MapCounter table_wordtype_id;
  for (auto iter = count_vector.begin(); iter != count_vector.end(); iter++) {
    std::string iter_str = iter->first;
    int iter_int = iter->second;
    table_wordtype_id.insert(PairCounter(iter_str, i_vocab));
    i_vocab++;

    if (i_vocab >= n_vocab) {
      std::cout << "min count:  " << iter_int << ", " << iter_str << std::endl;
      break;
    }
  }

  // Convert words to wordtype id
  unsigned long long n_oov = 0;
  tokens.resize(n_tokens);
  document_id.resize(n_tokens);

  convert_corpus_to_wordtype(path_corpus, table_wordtype_id, tokens, document_id, n_oov);

  // Display some informations
  std::cout << std::endl;
  std::cout << "Corpus      : " << path_corpus << std::endl;
  std::cout << "# of tokens : " << n_tokens << std::endl;
  std::cout << "# of doc.   : " << *std::max_element(document_id.begin(), document_id.end()) + 1 << std::endl;
  std::cout << "# of OOV    : " << n_oov << std::endl;
  std::cout << "# of vocab  : " << n_vocab << std::endl;
  std::cout << "Window size : " << window << std::endl;
  std::cout << "Coverage(%) : " << 100 * (n_tokens - n_oov) / (double)n_tokens << std::endl;
  std::cout << std::endl;
}

int main(int argc, const char** argv)
{
  // Parse command line arguments
  std::map<std::string, docopt::value> args
    = docopt::docopt(USAGE, { argv + 1, argv + argc }, true, "Kadingir (CL-Eigenwords) 1.0");

  const std::string path_corpus1 = args["--corpus1"].asString();
  const std::string path_corpus2 = args["--corpus2"].asString();
  const std::string path_output  = args["--output" ].asString();
  const int         n_vocab1     = args["--vocab1" ].asLong();
  const int         n_vocab2     = args["--vocab2" ].asLong();
  const int         window1      = args["--window1"].asLong();
  const int         window2      = args["--window2"].asLong();
  const int         dim          = args["--dim"    ].asLong();
  const bool        debug        = args["--debug"  ].asBool();

  std::vector<int> tokens1, tokens2, document_id1, document_id2;
  std::vector<PairCounter> count_vector1, count_vector2;
  set_tokens(path_corpus1, n_vocab1, window1, tokens1, document_id1, count_vector1);
  set_tokens(path_corpus2, n_vocab2, window2, tokens2, document_id2, count_vector2);

  std::cout << std::endl;
  std::cout << "Output      : " << path_output << std::endl;
  std::cout << "dim         : " << dim << std::endl;
  std::cout << std::endl;

  // Execute CL-Eigenwords
  const std::vector<int> window_sizes = { window1, window2 };
  const std::vector<int> vocab_sizes = { n_vocab1, n_vocab2 };
  const unsigned long long n_tokens1 = tokens1.size();
  const unsigned long long n_tokens2 = tokens2.size();
  const std::vector<unsigned long long> sentence_lengths = { n_tokens1, n_tokens2 };
  const std::vector<double> weight_vsdoc = { 1000.0, 1000.0 };
  tokens1.insert(tokens1.end(), tokens2.begin(), tokens2.end());
  document_id1.insert(document_id1.end(), document_id2.begin(), document_id2.end());

  CLEigenwords cleigenwords(tokens1, document_id1,
			    window_sizes, vocab_sizes,
			    sentence_lengths, dim,
			    true,
			    weight_vsdoc,
			    debug);
  cleigenwords.compute(2*dim + 10);
  const MatrixXd vectors = cleigenwords.get_vector_representations();

  // Output vector representations as a txt file
  const int n_rep_lang1 = (2*window1 + 1) * n_vocab1;
  const int n_rep_lang2 = (2*window2 + 1) * n_vocab2;
  const int n_documents = *std::max_element(document_id1.begin(), document_id1.end()) + 1;
  std::vector<std::string> wordtypes(n_rep_lang1 + n_rep_lang2 + n_documents);

  for (int i = 0; i < vectors.rows(); i++) {
    if (i < n_rep_lang1) {
      const int i_word = i % n_vocab1;

      if (i_word == 0) {
        wordtypes[i] = "<OOV>";
      } else {
        wordtypes[i] = count_vector1[i_word - 1].first;
      }
    } else if (i < n_rep_lang1 + n_rep_lang2) {
      const int i_word = (i - n_rep_lang1) % n_vocab2;

      if (i_word == 0) {
        wordtypes[i] = "<OOV>";
      } else {
        wordtypes[i] = count_vector2[i_word - 1].first;
      }
    } else {
      const int i_doc = i - n_rep_lang1 - n_rep_lang2;
      wordtypes[i] = std::to_string(i_doc);
    }
  }

  write_txt(path_output, wordtypes, vectors, n_vocab1 + n_vocab2, dim);


  return 0;
}
