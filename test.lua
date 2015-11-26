local xml = require('LuaXML')

-- load XML data from file "test.xml" into local table xfile
local xfile = xml.load("test.xml")
-- search for substatement having the tag "scene"
local xscene = xfile:find("scene")
-- if this substatement is found...
if xscene ~= nil then
  --  ...print it to screen
  print(xscene)
  --  print  tag, attribute id and first substatement
  print( xscene:tag(), xscene.id, xscene[1] )
end

-- demonstrate iterating the "resources" children with a certain MIME type
for k, v in xfile:find("resources"):children(nil, "mime", "model/vrml") do
	print((string.gsub(v:str(), "\n", "")))
end
-- since the attribute match is unambiguous, we can even use xfile recursively
for k, v in xfile:children(nil, "mime", "image/png", true) do
	print(v:tag(), v.name, v.id, v.mime, v.url)
end

-- demonstrate a more complex callback: iterate() over all elements having
-- an "id" attribute, and show those where it starts with "fa" or "fg"
xfile:iterate(
	function(var, depth)
		if var.id:match("^f[ag]") then
			print(var:tag(), var.id, var[1])
		end
	end,
nil, "id", nil, true)

xfile:save"t.xml"
print("---\nREADY.")
