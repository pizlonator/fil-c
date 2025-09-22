# Web serving with/without privilege

## Building

This sample program needs to be built as follows (when built with Go
prior to 1.15):
```
   export CGO_LDFLAGS_ALLOW="-Wl,-?-wrap[=,][^-.@][^,]*"
   go mod tidy
   go build web.go
```
go1.15+ does not require the `CGO_LDFLAGS_ALLOW` environment variable
and can build this code with:
```
   go mod tidy
   go build web.go
```

## Further discussion

A more complete walk through of what this code does is provided on the
[Fully Capable
website](https://sites.google.com/site/fullycapable/getting-started-with-go/building-go-programs-that-manipulate-capabilities).

## Reporting bugs

Go compilers prior to go1.11.13 are not expected to work. Report more
recent issues to the [`libcap` bug tracker](https://bugzilla.kernel.org/buglist.cgi?component=libcap&list_id=1065141&product=Tools&resolution=---).
