local PlanetWars = {}
PlanetWars.__index = PlanetWars

function PlanetWars:num_planets()
  return #self.planets
end

function PlanetWars:get_planet(planet_id)
  return self.planets[planet_id + 1]
end

function PlanetWars:num_fleets()
  return #self.fleets
end

function PlanetWars:get_fleet(fleet_id)
  return self.fleets[fleet_id + 1]
end

function PlanetWars:my_planets()
  local r = {}
  for _, p in ipairs(self.planets) do
    if p.owner == 1 then
      r[#r + 1] = p
    end
  end
  return r
end

function PlanetWars:neutral_planets()
  local r = {}
  for _, p in ipairs(self.planets) do
    if p.owner == 0 then
      r[#r + 1] = p
    end
  end
  return r
end

function PlanetWars:enemy_planets()
  local r = {}
  for _, p in ipairs(self.planets) do
    if p.owner > 1 then
      r[#r + 1] = p
    end
  end
  return r
end

function PlanetWars:not_my_planets()
  local r = {}
  for _, p in ipairs(self.planets) do
    if p.owner ~= 1 then
      r[#r + 1] = p
    end
  end
  return r
end

function PlanetWars:my_fleets()
  local r = {}
  for _, f in ipairs(self.fleets) do
    if f.owner == 1 then
      r[#r + 1] = f
    end
  end
  return r
end

function PlanetWars:enemy_fleets()
  local r = {}
  for _, f in ipairs(self.fleets) do
    if f.owner > 1 then
      r[#r + 1] = f
    end
  end
  return r
end

function PlanetWars:distance(source, destination)
  local s = type(source) == "table" and source or self.planets[source + 1]
  local d = type(destination) == "table" and destination or self.planets[destination + 1]
  local dx = s.x - d.x
  local dy = s.y - d.y
  return math.ceil(math.sqrt(dx * dx + dy * dy))
end

function PlanetWars:is_alive(player_id)
  for _, p in ipairs(self.planets) do
    if p.owner == player_id then
      return true
    end
  end
  for _, f in ipairs(self.fleets) do
    if f.owner == player_id then
      return true
    end
  end
  return false
end

-- The following methods are injected by the C engine:
--
-- pw:issue_order(source, destination, num_ships)
--   source: integer planet_id or planet table
--   destination: integer planet_id or planet table
--   num_ships: integer number of ships to send
--
-- pw:debug(message)
--   message: string, logged to engine console

return PlanetWars
