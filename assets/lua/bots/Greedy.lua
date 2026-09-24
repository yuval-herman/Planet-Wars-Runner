-- Greedy: from each planet, send half its ships to the closest capturable target.
-- Prefers weak planets (fewer ships needed) but also considers distance.
-- Only attacks when it has at least 2 ships to spare.

function do_turn(pw)
  local targets = pw:not_my_planets()
  if #targets == 0 then return end

  for _, src in ipairs(pw:my_planets()) do
    local ships_to_send = math.floor(src.num_ships / 2)
    if ships_to_send < 1 then goto continue end

    -- Score each target: prefer cheap (few ships) and close (short trip)
    local best, best_score = nil, -math.huge
    for _, dst in ipairs(targets) do
      local dist = pw:distance(src, dst)
      -- Avoid sending fleets we know can't win
      if ships_to_send > dst.num_ships then
        local score = dst.growth_rate * 10 - dst.num_ships - dist * 0.5
        if score > best_score then
          best_score = score
          best = dst
        end
      end
    end

    if best then
      pw:issue_order(src, best, ships_to_send)
    end

    ::continue::
  end
end
