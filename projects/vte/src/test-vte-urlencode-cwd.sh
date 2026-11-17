#!/usr/bin/env bash
# Copyright © 2013 Christian Persch
# Copyright © 2019 Red Hat, Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# Red Hat Author(s): Carlos Santos

set -e

vte_urlencode_cwd="$1"

test -x "$vte_urlencode_cwd" || exit 126

export PWD

check_urlencode() {
  PWD=$(echo -ne "$1")
  encoded=$("$vte_urlencode_cwd")
  if test "$encoded" != "$2"; then
    echo "FAILED:"
    echo "Input   : \"$PWD\""
    echo "Output  : \"$encoded\""
    echo "Expected: \"$2\""
  fi
}

# raw bytes

check_urlencode "\001\002\003\004\005\006\007\010\011\012\013\014\015\016\017" \
                "%01%02%03%04%05%06%07%08%09%0A%0B%0C%0D%0E%0F"
check_urlencode "\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036\037" \
                "%10%11%12%13%14%15%16%17%18%19%1A%1B%1C%1D%1E%1F"
check_urlencode "\040\041\042\043\044\045\046\047\050\051\052\053\054\055\056\057" \
                "%20!%22%23%24%25%26'()%2A%2B%2C-./"
check_urlencode "\060\061\062\063\064\065\066\067\070\071\072\073\074\075\076\077" \
                "0123456789:%3B%3C%3D%3E%3F"
check_urlencode "\0100\0101\0102\0103\0104\0105\0106\0107\0110\0111\0112\0113\0114\0115\0116\0117" \
                "%40ABCDEFGHIJKLMNO"
check_urlencode "\0120\0121\0122\0123\0124\0125\0126\0127\0130\0131\0132\0133\0134\0135\0136\0137" \
                "PQRSTUVWXYZ%5B%5C%5D%5E_"
check_urlencode "\0140\0141\0142\0143\0144\0145\0146\0147\0150\0151\0152\0153\0154\0155\0156\0157" \
                "%60abcdefghijklmno"
check_urlencode "\0160\0161\0162\0163\0164\0165\0166\0167\0170\0171\0172\0173\0174\0175\0176\0177" \
                "pqrstuvwxyz%7B%7C%7D~%7F"
check_urlencode "\0200\0201\0202\0203\0204\0205\0206\0207\0210\0211\0212\0213\0214\0215\0216\0217" \
                "%80%81%82%83%84%85%86%87%88%89%8A%8B%8C%8D%8E%8F"
check_urlencode "\0220\0221\0222\0223\0224\0225\0226\0227\0230\0231\0232\0233\0234\0235\0236\0237" \
                "%90%91%92%93%94%95%96%97%98%99%9A%9B%9C%9D%9E%9F"
check_urlencode "\0240\0241\0242\0243\0244\0245\0246\0247\0250\0251\0252\0253\0254\0255\0256\0257" \
                "%A0%A1%A2%A3%A4%A5%A6%A7%A8%A9%AA%AB%AC%AD%AE%AF"
check_urlencode "\0260\0261\0262\0263\0264\0265\0266\0267\0270\0271\0272\0273\0274\0275\0276\0277" \
                "%B0%B1%B2%B3%B4%B5%B6%B7%B8%B9%BA%BB%BC%BD%BE%BF"
check_urlencode "\0300\0301\0302\0303\0304\0305\0306\0307\0310\0311\0312\0313\0314\0315\0316\0317" \
                "%C0%C1%C2%C3%C4%C5%C6%C7%C8%C9%CA%CB%CC%CD%CE%CF"
check_urlencode "\0320\0321\0322\0323\0324\0325\0326\0327\0330\0331\0332\0333\0334\0335\0336\0337" \
                "%D0%D1%D2%D3%D4%D5%D6%D7%D8%D9%DA%DB%DC%DD%DE%DF"
check_urlencode "\0340\0341\0342\0343\0344\0345\0346\0347\0350\0351\0352\0353\0354\0355\0356\0357" \
                "%E0%E1%E2%E3%E4%E5%E6%E7%E8%E9%EA%EB%EC%ED%EE%EF"
check_urlencode "\0360\0361\0362\0363\0364\0365\0366\0367\0370\0371\0372\0373\0374\0375\0376\0377" \
                "%F0%F1%F2%F3%F4%F5%F6%F7%F8%F9%FA%FB%FC%FD%FE%FF"

# UTF-8

check_urlencode "ẞ" "%E1%BA%9E"

# all tests pass!
exit 0
