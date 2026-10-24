__attribute__((visibility("hidden"))) int hidden_function(void);

/* Exercise the signature-matching fallback thunk in a separate TU. */
__attribute__((visibility("default"))) int cross_tu(void)
{
    return hidden_function();
}
