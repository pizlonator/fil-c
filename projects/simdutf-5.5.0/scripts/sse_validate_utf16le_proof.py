# Note: Validation is done for 8-word input, we just need to check 3^8 = 6561 cases
#       Validation for 16-word inputs reqires 3**16 = 43'046'721 checks

ELEMENTS_COUNT   = 8
ALL_MASK         = (1 << ELEMENTS_COUNT) - 1
ALL_BUT_ONE_MASK = (ALL_MASK >> 1)

# 'V' - single-word character (always valid)
# 'L' - low surrogate (must be followed by the high surrogate)
# 'H' - high surrogate
def all_sequences():
    index = ['V'] * ELEMENTS_COUNT

    def increment():
        nonlocal index
        for i in range(ELEMENTS_COUNT):
            if index[i] == 'V':
                index[i] = 'L'
                return False

            if index[i] == 'L':
                index[i] = 'H'
                return False

            if index[i] == 'H':
                index[i] = 'V' # wrap around
                pass

        return True

    overflow = False
    while not overflow:
        yield index
        overflow = increment()


def find_error_in_words(words):
    prev = None

    if words[0] == 'H':
        # We assume that our vector algorithm loads proper data into vectors.
        # In the case low surrogate was the last item in the previous iteration.
        return 'high surrogate must not start a chunk'

    for i, kind in enumerate(words):
        if kind == 'V':
            if prev == 'L':
                return f'low surrogate {i - 1} must be followed by high surrogate'
        elif kind == 'L':
            if prev == 'L':
                return f'low surrogate {i - 1} must be followed by high surrogate'
        elif kind == 'H':
            if prev != 'L':
                return f'high surrogate {i} must be preceded by low surrogate'

        prev = kind

    return ''


def bitmask(words, state):
    result = 0
    for bit, type in enumerate(words):
        if type == state:
            # In SSE vector algorithm we compare 2 x 16 higher bytes of input
            # words, which yields a 16-bit mask.
            result |= 1 << bit

    return result


def mask(words):
    L = bitmask(words, 'L')
    H = bitmask(words, 'H')
    V = (~(L | H)) & ALL_MASK

    a = L & (H >> 1)
    b = a << 1
    c = V | a | b

    return c


def dump():
    for words in all_sequences():

        c = mask(words)

        words_image = "[ %s ]" % ' | '.join(words)
        error = find_error_in_words(words)
        if error == '':
            valid_image = 'T'
        else:
            valid_image = ' '

        print(words_image, valid_image, '{:016b} {:04x}'.format(c, c))


def proof():
    case1_hit = False
    case2_hit = False
    for words in all_sequences():

        c = mask(words)

        if c == ALL_MASK:
            case1_hit = True
            # all 16 words are valid (either 'V' or pairs 'L', 'H')
            assert find_error_in_words(words) == '', (words, find_error_in_words(words))

        if c == ALL_BUT_ONE_MASK:
            case2_hit = True
            # all 15 words are valid (either 'V' or pairs 'L', 'H')
            # the last words is either 'L' or 'H' (the word will be
            # re-examined in the next iteration of an algorithm)
            if words[-1] == 'H':
                assert find_error_in_words(words) == 'high surrogate 7 must be preceded by low surrogate'
            elif words[-1] == 'L':
                assert find_error_in_words(words) == ''
            else:
                assert False

    assert case1_hit
    assert case2_hit

    print("All OK")


def main():
    if 0:
        dump()
    else:
        proof()


if __name__ == '__main__':
    main()
