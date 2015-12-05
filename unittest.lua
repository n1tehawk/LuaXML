-- unit tests for LuaXML

local lu = require('luaunit')
local xml = require('LuaXML')

TestXml = {} -- the test suite

function TestXml:test_basics()
	-- encoding / decoding XML representations
	lu.assertEquals(xml.encode("\128"), "&#128;")
	lu.assertEquals(xml.decode("&#128;"), "\128")
	lu.assertEquals(xml.encode("<->"), "&lt;-&gt;")
	lu.assertEquals(xml.decode("&lt;-&gt;"), "<->")

	-- check metatable of newly created object
	local foobar = xml.new()
	lu.assertIsTable(foobar)
	lu.assertEquals(getmetatable(foobar).__index, xml)

	-- simple XML strings
	lu.assertNil(xml.eval(""))
	foobar = xml.eval('<foo bar="true" />')
	lu.assertIsTable(foobar)
	lu.assertEquals(foobar[0], "foo")
	lu.assertEquals(foobar.bar, "true")

	-- direct conversion (XML 'encapsulation') of Lua values
	lu.assertEquals(xml.str(true), "<boolean>true</boolean>\n")
	lu.assertEquals(xml.str(123), "<number>123</number>\n")
	lu.assertEquals(xml.str("bar"), "<string>bar</string>\n")
	-- with setting the tag at the same time
	lu.assertEquals(xml.str("bar", nil, "foo"), "<foo>bar</foo>\n")

	-- construct new XML object from scratch, and call a method
	foobar = xml.new({bar = false}, "foo")
	lu.assertEquals(foobar:str(), '<foo bar="false" />\n')

	-- mimic the append() example from the README
	foobar = xml.new("root")
	foobar:append("child")[1] = 123
	lu.assertEquals(foobar[1]:str(), "<child>123</child>\n")

	-- proper handling of an empty attribute
	local foo = '<tag attr="" />\n'
	foobar = xml.eval(foo)
	lu.assertEquals(foobar.attr, "")
	lu.assertEquals(foobar:str(), foo)

	-- encoding / decoding of special entities
	foo = xml.new({"<&>"}, "foo")
	lu.assertEquals(foo:str(), "<foo>&lt;&amp;&gt;</foo>\n")
	lu.assertEquals(xml.eval("<foo>&#x20;</foo>")[1], " ") -- hexadecimal
	lu.assertEquals(xml.eval("<foo>&#032;</foo>")[1], " ")
	lu.assertEquals(xml.eval("<bar>&#32;</bar>")[1], " ")
	lu.assertEquals(xml.eval("<foobar>&apos;&#9;&apos;</foobar>")[1], "'\t'")

	-- custom encodings
	xml.registerCode("\160", " ")
	lu.assertEquals(xml.encode("\160"), " ")
	xml.registerCode("\160", "&nbsp;") -- replace existing entry
	lu.assertEquals(xml.encode("\160"), "&nbsp;")
	lu.assertEquals(xml.eval("<foo>&nbsp;</foo>")[1], "\160")
	xml.registerCode("\160", nil) -- remove entry (revert to standard encoding)
	lu.assertEquals(xml.encode("\160"), "&#160;")

	-- enhanced whitespace handling
	lu.assertEquals(xml.eval("<foo> </foo>"):str(), "<foo />\n") -- default mode
	lu.assertEquals(xml.eval("<foo> </foo>", xml.WS_TRIM):str(), "<foo />\n")
	lu.assertEquals(xml.eval("<foo> </foo>", xml.WS_PRESERVE):str(),
					"<foo> </foo>\n")
	lu.assertEquals(xml.eval("<foo>\n  <bar/> x\t</foo>", xml.WS_TRIM),
					{{[0]="bar"}, "x", [0]="foo"})
	lu.assertEquals(xml.eval("<foo>\n  <bar/> x\t</foo>", xml.WS_NORMALIZE),
					{{[0]="bar"}, " x\t", [0]="foo"})
	lu.assertEquals(xml.eval("<foo>\n  <bar/> x\t</foo>", xml.WS_PRESERVE),
					{"\n  ", {[0]="bar"}, " x\t", [0]="foo"})

	-- CDATA
	lu.assertEquals(xml.eval("<fu><![CDATA[]]></fu>"), {[0] = "fu"})
	lu.assertEquals(xml.eval("<fu>foo<![CDATA[]]>bar</fu>"),
		{"foo", "bar", [0] = "fu"})
	lu.assertEquals(xml.eval("<fu>foo<![CDATA[foobar]]>bar</fu>"),
		{"foo", "foobar", "bar", [0] = "fu"})

	-- invalid XML
	lu.assertErrorMsgContains("Malformed XML", xml.eval, "foo<bar/>")

	-- UTF-8 BOM (byte order mark)
	foobar = xml.eval("\239\187\191<foo/>")
	lu.assertEquals(foobar[0], "foo")

	-- check load error
	lu.assertErrorMsgContains("file error or file not found",
		xml.load, "invalid_filename")

	-- safeguard against global namespace pollution
	lu.assertNil(_G.xml)
end

function TestXml:test_parse()
	local test = xml.load("test.xml")

	local scene = test:find("scene")
	lu.assertIsTable(scene)
	lu.assertEquals(scene:tag(), "scene") -- XML tag
	lu.assertEquals(scene.id, "0") -- XML property/attribute
	lu.assertEquals(scene.script, "main")

	-- first (sub)element
	scene = scene[1]
	lu.assertIsTable(scene)
	lu.assertEquals(scene:tag(), "object")
	lu.assertEquals(scene.name, "observer")
	lu.assertEquals(scene.id, "0")
	lu.assertEquals(scene.input, "window")
	lu.assertEquals(scene.script, "camera.lua")

	-- make sure that the CDATA element from <script> starts with '\n' and
	-- ends with ' ' (i.e. preserved whitespace)
	local script_str = test:find("script")[1]
	lu.assertEquals(script_str:sub(1, 1), '\n')
	lu.assertEquals(script_str:sub(-1), ' ')

	-- more CDATA tests
	local cdata_test = test:find("cdata_test")
	lu.assertEquals(cdata_test:find("tagged")[1], "<works>")
	lu.assertEquals(cdata_test:find("chars")[1], "a  <b>\tc")
	lu.assertEquals(cdata_test:find("amp")[1], "&amp;")
	lu.assertEquals(cdata_test:find("open")[1], "<")
	lu.assertEquals(cdata_test:find("close")[1], ">")
	lu.assertEquals(cdata_test:find("empty"), {[0] = "empty"})

	-- test match() / iterate() functions against known values
	lu.assertEquals(test:find(nil, "id", "dopplerVelocity")[1], "330.0")
	local func, t = test:find("resources"):children(nil, "mime", "audio/wav")
	lu.assertEquals(#t, 3)
	-- verify number of elements with "float" tag
	lu.assertEquals(test:iterate(function() end, "float", nil, nil, true), 14)
	-- verify number of elements with "id" attribute
	lu.assertEquals(test:iterate(function() end, nil, "id", nil, true), 58)
	-- verify number of elements where "loop" attribute is "true"
	lu.assertEquals(test:iterate(function() end, nil, "loop", "true", true), 4)
end

function TestXml:test_transform()
	local test = xml.load("test.xml")

	-- There is a slight inconsistency in the output produced by LuaXML:
	-- It doesn't preserve the CDATA encapsulation within the <script> element,
	-- which in turn causes xml.load() to discard whitespace enclosing the
	-- value. The two tables won't match unless we do the same for the original.
	local script = test:find("script")
	script[1] = string.gsub(script[1], "^[ \t\n]+", "")
	script[1] = string.gsub(script[1], "[ \t\n]+$", "")
	-- make sure we can parse back the textual representation without "losses"
	local expected = xml.eval(test:str())
	lu.assertEquals(test, expected)

	-- But the test *will* pass smoothly with modified whitespace handling!
	test = xml.load("test.xml")
	expected = xml.eval(test:str(), xml.WS_NORMALIZE)
	lu.assertEquals(test, expected)
end

-- run test suite with verbose output
os.exit(lu.LuaUnit.run("-v"))
