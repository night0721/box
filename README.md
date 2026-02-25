# box
Sandbox container to run untrusted programs with isolated namespaces and minimal cgroups

# Features
- OverlayFS
- Limit resource
- Virtual networking

# Usage
```sh
curl -O https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.0-x86_64.tar.gz
tar xf alpine-minirootfs-3.20.0-x86_64.tar.gz
box
```

# Dependencies
None

# Building
You will need to run these with elevated privilages.
```
$ make 
# make install
```

# Contributions
Contributions are welcomed, feel free to open a pull request.

# License
This project is licensed under the GNU Public License v3.0. See [LICENSE](https://github.com/night0721/box/blob/master/LICENSE) for more information.
