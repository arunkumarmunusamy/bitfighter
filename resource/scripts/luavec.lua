point = {}

-- For saving before sandbox wipes these out
local tms = setmetatable
local tmg = getmetatable

local mt = {}

function point.new(x,y)
  local x=x or 0
  local y=y or 0
  local v={x=x,y=y}
  tms(v,mt)
  return v
end

function point.dot(v1,v2)
  return v1.x*v2.x+v1.y*v2.y
end

function point.cross(v1,v2)
  return point.new(v1.x*v2.y-v1.y*v2.x,0)
end

function point.normalize(v)
  local s=1.0/math.sqrt(v.x*v.x+v.y*v.y);
  return point.new(v.x*s,v.y*s)
end

function point.length(v)
  return math.sqrt(v.x*v.x+v.y*v.y)
end

function point.lengthSquared(v)
  return v.x*v.x+v.y*v.y
end

function point.angleTo(v1,v2)
  return math.atan2(v2.y-v1.y,v2.x-v1.x)
end

function point.distanceTo(v1,v2)
  return math.sqrt((v2.x-v1.x)*(v2.x-v1.x)+(v2.y-v1.y)*(v2.y-v1.y))
end

function point.distSquared(v1,v2)
  return (v2.x-v1.x)*(v2.x-v1.x)+(v2.y-v1.y)*(v2.y-v1.y)
end

-- Some possibly useful constants
point.zero = point.new(0,0)
point.one = point.new(1,1)


-- Metamethods for a 'point'

-- Pretty printing of a point
mt.__tostring = function(p) return "point ("..tostring(p.x)..","..tostring(p.y)..")"  end

-- Math operators
mt.__add = function(v1,v2) return point.new(v1.x+v2.x,v1.y+v2.y) end
mt.__sub = function(v1,v2) return point.new(v1.x-v2.x,v1.y-v2.y) end
mt.__mul = function(v1,v2)
  local s = tonumber(v2)
  if s then
    -- vector * scalar
    return point.new(v1.x*s,v1.y*s)
  else
    local s = tonumber(v1)
    if s then
      -- scalar * vector
      return point.new(v2.x*s,v2.y*s)
    else
      -- vector * vector
      return point.new(v1.x*v2.x,v1.y*v2.y)
	end
  end
end
mt.__div = function(v1,s) return point.new(v1.x/s,v1.y/s) end
mt.__unm = function(v) return point.new(-v.x,-v.y) end

-- Patch 'type' function
local t = type
type = function(o)
  -- Old type
  local ot = t(o)
  if ot == "table" and tmg(o) == mt then return "point" end
  return ot
end