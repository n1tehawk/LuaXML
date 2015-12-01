[![License](http://img.shields.io/badge/License-MIT-green.svg)](#luaxml-license)
[![Build status](https://ci.appveyor.com/api/projects/status/d93a5k3b1xa8sb3r?svg=true)](https://ci.appveyor.com/project/n1tehawk/luaxml)
[![Build Status](https://travis-ci.org/n1tehawk/LuaXML.svg?branch=master)](https://travis-ci.org/n1tehawk/LuaXML)

## LuaXML

- A module that maps between Lua and XML without much ado
- Version 1.8.0 (Lua 5.2), 2013-06-10 by Gerald Franz, _eludi.net_

LuaXML provides a minimal set of functions for the processing of XML data in Lua. It offers a 
very simple and natural mapping between the XML data format and Lua tables, which 
allows one to parse XML data just using Lua's normal table access and iteration 
methods:

Substatements and text content is represented as array data having 
numerical keys, attributes and tags use string keys. This representation makes 
sure that the structure of XML data is exactly preserved in a read/write cycle. 

Since version 1.7, LuaXML consists of a well-optimized portable ISO-standard C file 
and a small Lua file. It is published under the same liberal licensing conditions as Lua 
itself (see below). It has been successfully compiled and used under Linux, various 
flavours of MS Windows, and MacOS X.



### Example

```lua
-- import the LuaXML module
xml = require("LuaXML")
-- load XML data from file "test.xml" into local table xfile
local xfile = xml.load("test.xml")
-- search for substatement having the tag "scene"
local xscene = xfile:find("scene")
-- if this substatement is found...
if xscene ~= nil then
  --  ...print it to screen
  print(xscene)
  --  print attribute id and first substatement
  print( xscene.id, xscene[1] )
  -- set attribute id
  xscene["id"] = "newId"
end

-- create a new XML object and set its tag to "root"
local x = xml.new("root")
-- append a new subordinate XML object, set its tag to "child", and its content to 123
x:append("child")[1] = 123
print(x)
```


### Documentation

See the [LuaXML reference](http://cdn.rawgit.com/n1tehawk/LuaXML/master/LuaXML.html)
for a list of available functions.


### LuaXML License

LuaXML is licensed under the terms of the MIT license reproduced below,
the same as Lua itself. This means that LuaXML is free software and can be
used for both academic and commercial purposes at absolutely no cost.

Copyright (C) 2007-2013 by Gerald Franz, eludi.net

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
