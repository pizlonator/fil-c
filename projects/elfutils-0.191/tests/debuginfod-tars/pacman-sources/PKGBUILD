pkgname=hello
pkgver=1
pkgrel=1
pkgdesc="Simple hello world program to exercise debuginfod"
arch=('x86_64')
source=('hello.c')
sha256sums=('f85badd2007451bbda4791e7fe820b41be0a424172a567573511688bff975235')

# guarantee that split debug packages are turned on
options=('strip' 'debug')

build() {
    # implicit Makefile
    make hello
}

package() {
    install -Dm755 hello "${pkgdir}"/usr/bin/hello
}
