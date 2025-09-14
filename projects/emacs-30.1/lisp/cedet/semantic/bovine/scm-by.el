;;; semantic/bovine/scm-by.el --- Generated parser support file  -*- lexical-binding:t -*-

;; Copyright (C) 2001-2025 Free Software Foundation, Inc.

;; This file is part of GNU Emacs.

;; GNU Emacs is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs.  If not, see <https://www.gnu.org/licenses/>.

;;; Commentary:
;;
;; This file was generated from admin/grammars/scheme.by.

;;; Code:

(require 'semantic/lex)
(eval-when-compile (require 'semantic/bovine))

;;; Prologue
;;
(declare-function semantic-parse-region "semantic"
		  (start end &optional nonterminal depth returnonerror))

;;; Declarations
;;
(eval-and-compile (defconst semantic-scm-by--expected-conflicts
                    nil
                    "The number of expected shift/reduce conflicts in this grammar."))

(defconst semantic-scm-by--keyword-table
  (semantic-lex-make-keyword-table
   '(("define" . DEFINE) ("define-module" . DEFINE-MODULE)
     ("module" . MODULE) ("load" . LOAD))
   '(("load" summary "Function: (load \"filename\")")
     ("define-module" summary
      "Function: (define-module (name arg1 ...)) ")
     ("define" summary "Function: (define symbol expression)")))
  "Table of language keywords.")

(defconst semantic-scm-by--token-table
  (semantic-lex-make-type-table
   '(("close-paren" (CLOSEPAREN . ")")) ("open-paren" (OPENPAREN . "(")))
   'nil)
  "Table of lexical tokens.")

(defconst semantic-scm-by--parse-table
  `(
    (bovine-toplevel
     (scheme)
     ) ;; end bovine-toplevel

    (scheme
     (semantic-list
      ,(semantic-lambda
        (let
            (
             (expand
              (semantic-bovinate-from-nonterminal
               (car
                (nth 0 vals))
               (cdr
                (nth 0 vals))
               'scheme-list)))
          (cond
           (
            (semantic-tag-of-class-p expand
                                     'module)
            (semantic-tag-new-type
             (semantic-tag-name expand)
             "module"
             (semantic-parse-region
              (car
               (nth 0 vals))
              (cdr
               (nth 0 vals))
              'scheme
              1) nil))
           (t expand))))
      )
     ) ;; end scheme

    (scheme-list
     (open-paren
      "("
      scheme-in-list
      ,(semantic-lambda
        (nth 1 vals))
      )
     ) ;; end scheme-list

    (scheme-in-list
     (DEFINE
      symbol
      expression
      ,(semantic-lambda
        (semantic-tag-new-variable
         (nth 1 vals) nil
         (nth 2 vals)))
      )
     (DEFINE
      name-args
      opt-doc
      ,(semantic-lambda
        (semantic-tag-new-function
         (car
          (nth 1 vals)) nil
         (cdr
          (nth 1 vals))))
      )
     (DEFINE-MODULE
      name-args
      ,(semantic-lambda
        (semantic-tag-new-package
         (nth
          (length
           (nth 1 vals))
          (nth 1 vals)) nil))
      )
     (MODULE
      symbol
      ,(semantic-lambda
        (semantic-tag
         (nth 0 vals)
         'module :members nil))
      )
     (LOAD
      string
      ,(semantic-lambda
        (semantic-tag-new-include
         (file-name-nondirectory
          (read
           (nth 1 vals)))
         (read
          (nth 1 vals))))
      )
     (symbol
      sequence
      ,(semantic-lambda
        (semantic-tag-new-code
         (nth 0 vals) nil))
      )
     (;;EMPTY
      ,(semantic-lambda
        (semantic-tag
         "Bad Tag in Sexp"
         'error))
      )
     ) ;; end scheme-in-list

    (name-args
     (semantic-list
      ,(lambda (vals start end)
         (ignore vals start end)
         (semantic-bovinate-from-nonterminal
          (car
           (nth 0 vals))
          (cdr
           (nth 0 vals))
          'name-arg-list))
      )
     ) ;; end name-args

    (name-arg-list
     (open-paren
      "("
      name-arg-expand
      ,(semantic-lambda
        (nth 1 vals))
      )
     ) ;; end name-arg-list

    (name-arg-expand
     (symbol
      name-arg-expand
      ,(semantic-lambda
        (cons
         (nth 0 vals)
         (nth 1 vals)))
      )
     (;;EMPTY
      ,(semantic-lambda)
      )
     ) ;; end name-arg-expand

    (opt-doc
     (string)
     (;;EMPTY
      )
     ) ;; end opt-doc

    (sequence
     (expression
      sequence)
     (expression)
     ) ;; end sequence

    (expression
     (symbol)
     (semantic-list)
     (string)
     (number)
     ) ;; end expression
    )
  "Parser table.")

(defun semantic-scm-by--install-parser ()
  "Setup the Semantic Parser."
  (setq semantic--parse-table semantic-scm-by--parse-table
        semantic-debug-parser-source "scheme.by"
        semantic-debug-parser-class 'semantic-bovine-debug-parser
        semantic-debug-parser-debugger-source 'semantic/bovine/debug
        semantic-flex-keywords-obarray semantic-scm-by--keyword-table
        ))


;;; Analyzers
;;

;;; Epilogue
;;

(provide 'semantic/bovine/scm-by)

;; Local Variables:
;; version-control: never
;; no-update-autoloads: t
;; End:

;;; semantic/bovine/scm-by.el ends here
