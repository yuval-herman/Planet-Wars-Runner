-- Berserker: no strategy, no mercy, no survivors.
-- Every single ship it has gets thrown at the nearest enemy planet.
-- Reckless? Absolutely. Terrifying to play against? You'll see.

function do_turn(pw)
  local enemies = pw:enemy_planets()
  if #enemies == 0 then return end

  for _, src in ipairs(pw:my_planets()) do
    if src.num_ships <= 1 then goto continue end

    -- Find the closest enemy planet
    local closest, closest_dist = nil, math.huge
    for _, dst in ipairs(enemies) do
      local d = pw:distance(src, dst)
      if d < closest_dist then
        closest_dist = d
        closest = dst
      end
    end

    if closest then
      -- Send everything but 1 ship (so the planet isn't left empty)
      pw:issue_order(src, closest, src.num_ships - 1)
    end

    ::continue::
  end
end
