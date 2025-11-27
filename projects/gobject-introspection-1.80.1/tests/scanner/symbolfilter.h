#ifndef __SYMBOLFILTER_H__
#define __SYMBOLFILTER_H__

typedef struct _SymbolfilterObject SymbolfilterObject;

SymbolfilterObject *SymbolfilterObjectNew (void);
void SymbolfilterObjectFooMethod (SymbolfilterObject *self);
void SymbolfilterObjectFree (SymbolfilterObject *self);

#endif
