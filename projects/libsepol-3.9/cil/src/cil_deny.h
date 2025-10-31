/*
 * This file is public domain software, i.e. not copyrighted.
 *
 * Warranty Exclusion
 * ------------------
 * You agree that this software is a non-commercially developed program
 * that may contain "bugs" (as that term is used in the industry) and
 * that it may not function as intended. The software is licensed
 * "as is". NSA makes no, and hereby expressly disclaims all, warranties,
 * express, implied, statutory, or otherwise with respect to the software,
 * including noninfringement and the implied warranties of merchantability
 * and fitness for a particular purpose.
 *
 * Limitation of Liability
 *-----------------------
 * In no event will NSA be liable for any damages, including loss of data,
 * lost profits, cost of cover, or other special, incidental, consequential,
 * direct or indirect damages arising from the software or the use thereof,
 * however caused and on any theory of liability. This limitation will apply
 * even if NSA has been advised of the possibility of such damage. You
 * acknowledge that this is a reasonable allocation of risk.
 *
 * Original author: James Carter
 */

#ifndef CIL_DENY_H_
#define CIL_DENY_H_

int cil_classperms_list_match_any(const struct cil_list *cpl1, const struct cil_list *cpl2);
int cil_classperms_list_match_all(const struct cil_list *cpl1, const struct cil_list *cpl2);
void cil_classperms_list_copy(struct cil_list **new, const struct cil_list *old);
void cil_classperms_list_and(struct cil_list **result, const struct cil_list *cpl1, const struct cil_list *cpl2);
void cil_classperms_list_andnot(struct cil_list **result, const struct cil_list *cpl1, const struct cil_list *cpl2);
int cil_process_deny_rules_in_ast(struct cil_db *db);

#endif /* CIL_DENY_H_ */
