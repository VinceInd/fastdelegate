Based on Don Clugston's Impossibly Fast Delegates:
https://www.codeproject.com/Articles/7150/Member-Function-Pointers-and-the-Fastest-Possible

* Install the Conan package manager:
* Install pip if necessary:
      * sudo apt install python-pip
      * sudo yum install python-pip
  * Install conan:
      * sudo pip install conan

* Generate the build scripts
  * cd build
  * conan install ..
  * cmake .. -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
  * make -j4 all

* Undefined reference:
  * conan install .. --build missing -s compiler=gcc -s compiler.version=6.3 -s compiler.libcxx=libstdc++11

