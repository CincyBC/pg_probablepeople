/* tools/train_model.c - Standalone CRF training tool */
#include "../src/crf_trainer.h"
#include "../src/training_data_parser.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
  printf("Usage: %s [OPTIONS] <input_file> -o <output_file>\n", prog);
  printf("\nTrain a CRF model for name parsing.\n");
  printf("\nOptions:\n");
  printf("  -o, --output FILE      Output model file (required)\n");
  printf("  -t, --type TYPE        Model type: person, company, or generic\n");
  printf("  -p, --person FILE      Person training data (for generic model)\n");
  printf(
      "  -c, --company FILE     Company training data (for generic model)\n");
  printf("  --c2 VALUE             L2 regularization coefficient (default: "
         "1.0)\n");
  printf("  --max-iter VALUE       Maximum iterations (default: 100)\n");
  printf("  --epsilon VALUE        Convergence threshold (default: 0.0001)\n");
  printf("  -v, --verbose          Verbose output\n");
  printf("  -h, --help             Show this help\n");
  printf("\nExamples:\n");
  printf("  %s name_data/person_labeled.xml -o person.crfsuite\n", prog);
  printf("  %s -t generic -p person.xml -c company.xml -o generic.crfsuite\n",
         prog);
}

int main(int argc, char *argv[]) {
  char *output_file = NULL;
  char *input_file = NULL;
  char *person_file = NULL;
  char *company_file = NULL;
  char *model_type = "person";
  int verbose = 0;
  TrainingConfig config;

  init_training_config(&config);

  static struct option long_options[] = {
      {"output", required_argument, 0, 'o'},
      {"type", required_argument, 0, 't'},
      {"person", required_argument, 0, 'p'},
      {"company", required_argument, 0, 'c'},
      {"c2", required_argument, 0, 1001},
      {"max-iter", required_argument, 0, 1002},
      {"epsilon", required_argument, 0, 1003},
      {"verbose", no_argument, 0, 'v'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, "o:t:p:c:vh", long_options, NULL)) !=
         -1) {
    switch (opt) {
    case 'o':
      output_file = optarg;
      break;
    case 't':
      model_type = optarg;
      break;
    case 'p':
      person_file = optarg;
      break;
    case 'c':
      company_file = optarg;
      break;
    case 1001:
      config.c2 = atof(optarg);
      break;
    case 1002:
      config.max_iterations = atoi(optarg);
      break;
    case 1003:
      config.epsilon = atof(optarg);
      break;
    case 'v':
      verbose = 1;
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  /* Get positional argument */
  if (optind < argc) {
    input_file = argv[optind];
  }

  /* Validate arguments */
  if (!output_file) {
    fprintf(stderr, "Error: Output file is required (-o)\n");
    print_usage(argv[0]);
    return 1;
  }

  int ret = -1;

  if (strcmp(model_type, "generic") == 0) {
    /* Generic model requires both person and company data */
    if (!person_file || !company_file) {
      fprintf(stderr, "Error: Generic model requires both -p and -c options\n");
      return 1;
    }

    printf("Training GENERIC model...\n");
    printf("  Person data: %s\n", person_file);
    printf("  Company data: %s\n", company_file);
    printf("  Output: %s\n\n", output_file);

    ret = train_generic_model(person_file, company_file, output_file, &config);

  } else {
    /* Single-type model */
    if (!input_file) {
      fprintf(stderr, "Error: Input file is required\n");
      print_usage(argv[0]);
      return 1;
    }

    printf("Training %s model...\n", model_type);
    printf("  Input: %s\n", input_file);
    printf("  Output: %s\n\n", output_file);

    TrainingData *data = parse_training_file(input_file);
    if (!data) {
      fprintf(stderr, "Error: Failed to parse training file\n");
      return 1;
    }

    if (verbose) {
      print_training_summary(data);
      printf("\n");
    }

    ret = train_crf_model(data, output_file, &config);
    free_training_data(data);
  }

  return ret;
}
