-- DemoBot in Lua: ports the logic from DemoBot.py
function do_turn(pw)
  if #pw:my_fleets() >= 1 then
    return
  end

  local source = nil
  local source_score = -999999.0
  for _, p in ipairs(pw:my_planets()) do
    local score = p.num_ships
    if score > source_score then
      source_score = score
      source = p
    end
  end

  local dest = nil
  local dest_score = -999999.0
  for _, p in ipairs(pw:not_my_planets()) do
    local score = 1.0 / (1 + p.num_ships)
    if score > dest_score then
      dest_score = score
      dest = p
    end
  end

  if source and dest then
    local num_ships = math.floor(source.num_ships / 2)
    if num_ships > 0 then
      pw:issue_order(source, dest, num_ships)
    end
  end
end
