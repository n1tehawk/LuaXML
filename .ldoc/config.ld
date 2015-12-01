-- configuration file for LDoc documentation tool
-- see https://github.com/stevedonovan/LDoc

project = "LuaXML"
title = "LuaXML reference"

-- files to process
file = {
	"../LuaXML.lua",
	"../LuaXML_lib.c"
}

style = "." -- (use CSS template from .ldoc)
merge = true -- (common documentation for C module and Lua code)
sort = true

output = "LuaXML"
dir = ".." -- output to main directory
format = "discount" -- markdown support
