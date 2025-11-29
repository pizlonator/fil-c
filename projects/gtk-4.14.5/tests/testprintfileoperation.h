#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TEST_TYPE_PRINT_FILE_OPERATION    (test_print_file_operation_get_type ())
#define TEST_PRINT_FILE_OPERATION(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_PRINT_FILE_OPERATION, TestPrintFileOperation))
#define TEST_IS_PRINT_FILE_OPERATION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_PRINT_FILE_OPERATION))

typedef struct _TestPrintFileOperationClass   TestPrintFileOperationClass;
typedef struct _TestPrintFileOperationPrivate TestPrintFileOperationPrivate;
typedef struct _TestPrintFileOperation        TestPrintFileOperation;

struct _TestPrintFileOperation
{
  GtkPrintOperation parent_instance;

  /* < private > */
  char *filename;
  double font_size;
  int lines_per_page;

  char **lines;
  int num_lines;
  int num_pages;
};

struct _TestPrintFileOperationClass
{
  GtkPrintOperationClass parent_class;
};

GType                   test_print_file_operation_get_type      (void);
TestPrintFileOperation *test_print_file_operation_new           (const char             *filename);
void                    test_print_file_operation_set_font_size (TestPrintFileOperation *op,
                                                                 double                  points);

G_END_DECLS
