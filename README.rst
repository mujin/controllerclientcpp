MUJIN Controller C++ Client Library
-----------------------------------

This is an open-source client library communicating with the MUJIN Controller WebAPI.

Uses  for communication. `cURL C Bindings <http://curl.haxx.se/libcurl/c/>`_

In the following documentation %MUJINCLIENTGIT% means the root directory where the sources are checked out.

Running on Windows
------------------

By default, library is installed in ``C:\Program Files\mujincontrollerclient``.

The ``bin`` directory holds all the DLLs and several sample exe programs. 

In order to use the library, link with ``lib\mujincontrollerclient-vcXX-mt.lib`` and add ``include`` in the MSVC include directories.

Need to put ``C:\Program Files\mujincontrollerclient\bin`` in the PATH environment variable, or register all the DLLs with windows system.

Building on Windows
-------------------

1. Checkout the source code and install

  - Download and install `msysGit <http://code.google.com/p/msysgit/downloads/list?q=full+installer+official+git>`_
  - (Optional for nice graphical interface) Download and install  `TortoiseGit <http://code.google.com/p/tortoisegit/wiki/Download>`_ 
  - Checkout the following git repository **https://github.com/mujin/controllerclientcpp.git**

2. Download and install `CMake <http://www.cmake.org/cmake/resources/software.html>`_ (>= v2.8.10)

3. Run CMake on ``%MUJINCLIENTGIT%``, choose the correct Visual Studio version for the Generator.

.. image:: https://raw.github.com/mujin/controllerclientcpp/master/docs/build_cmake.png

Can modify the ``CMAKE_INSTALL_PREFIX`` variable in order to change the install directory. Default is ``C:\Program Files``

4. Open the **build/mujincontrollerclientcpp.sln** solution and compile the **ALL_BUILD** project.

.. image:: https://raw.github.com/mujin/controllerclientcpp/master/docs/build_visualstudio.png

5. To build the project using the Visual Studio Command Prompt::

    cd %MUJINCLIENTGIT%\build
    msbuild mujincontrollerclient.sln /p:Configuration=Release

6. In order to Install into ``c:\Program Files``, compile the **INSTALL** project. For Visual Studio 9 2008 and above use::
  
    msbuild INSTALL.vcxproj /p:Configuration=Release
  
  otherwise use::
  
    msbuild INSTALL.vcproj /p:Configuration=Release


Building OpenSSL
================

If OpenSSL libraries do not exist for the specific Visual Studio version


1. Download and Install `Starberry Perl <http://strawberryperl.com/>`_

2. Download and Install `NASM <http://sourceforge.net/projects/nasm/files/Win32%20binaries/2.07/nasm-2.07-installer.exe/download>`_

  - add ``C:\Program Files\NASM`` to the **PATH** variable.

3. Uncompress **openssl-1.0.1c.tar.gz**.

4. Open the Visual Studio Command Prompt, cd into ``openssl-1.0.1c``, set the XX depending on the VC++ version, and run::

    perl Configure VC-WIN32 --prefix=%MUJINCLIENTGIT%\msvc_binaries\vcXX
    ms\do_nasm
    nmake -f ms\ntdll.mak
    nmake -f ms\ntdll.mak install

5. The final binaries should be in the ``msvc_binaries\vcXX\lib`` folder.

Building libcurl
================

If libcurl libraries do not exist for the specific Visual Studio version

1. Uncompress ``curl-7.28.1-patched.tar.gz``

2. In the Visual Studio Command Prompt and cd into ``%MUJINCLIENTGIT%/curl-7.28.1``. Create and compile the project with the following command::

    mkdir buildvcXX
    cd buildvcXX
    cmake -DOPENSSL_ROOT_DIR=%MUJINCLIENTGIT%\msvc_binaries\vcXX -DCMAKE_REQUIRED_INCLUDES=%MUJINCLIENTGIT%\msvc_binaries\vcXX\include -DBUILD_CURL_TESTS=OFF -DCURL_USE_ARES=OFF -DCURL_STATICLIB=OFF -DCMAKE_INSTALL_PREFIX=%MUJINCLIENTGIT%\msvc_binaries\vcXX -G "Visual Studio XX" ..
    msbuild CURL.sln /p:Configuration=Release

3. To install, for Visual Studio 9 2008 and above use::
  
    msbuild INSTALL.vcxproj /p:Configuration=Release

  otherwise use::

    msbuild INSTALL.vcproj /p:Configuration=Release

where "Visual Studio XX" is the cmake generator for visual studio. for example: "Visual Studio 8 2005" or "Visual Studio 10". 

Updating the Windows Libraries
------------------------------

Several libraries are being managed in this repository. If necessary, get upgraded versions from the following places:

1. `boost <http://www.boostpro.com/download/>`_ (any version >= 1.40 is fine).

  - Select Multi-threaded DLL libraries.
  - No extra libraries need to be selected, only the header files.
  
  There is a default included boost (v1.44) if one cannot be detected.

2. - `cURL <http://curl.haxx.se/libcurl/>`_

  - The patches applied to curl are written in ``curl-7.28.1.patches``

3. - `OpenSSL <http://www.openssl.org>`_

  - Once updated, cURL has to be recompiled just to make sure the symbols match.

Licenses
--------

MUJIN Controller C++ Client is Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

In other words, **commercial use and any modifications are allowed**.

Since OpenSSL is included, have to insert the following statement in commercial products::

  This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit. (http://www.openssl.org/)


Other Possible Clients
======================

- `cpp-netlib <http://cpp-netlib.github.com/latest/index.html>`_ - uses boost asio and cmake. `Using wiith https <https://groups.google.com/forum/?fromgroups=#!topic/cpp-netlib/M8LIz9ahMLo>`_ requires at least v0.9.4.

- `Windows HTTP Services <http://msdn.microsoft.com/en-us/library/aa384273%28VS.85%29.aspx?ppud=4>`_

- `libcurl.NET <http://sourceforge.net/projects/libcurl-net/>`_ - Windows only
