-- Defender: a careful tactician who never overextends.
-- Priority 1: Reinforce any planet under incoming enemy attack.
-- Priority 2: Only attack enemy planets it can overwhelm with a 2x force.
-- It won't rush you, but try sneaking past its walls.

function do_turn(pw)
  local my_planets = pw:my_planets()
  if #my_planets == 0 then return end

  -- Tally incoming ships per planet (enemy fleets en route to ours)
  local incoming = {}
  for _, f in ipairs(pw:enemy_fleets()) do
    local id = f.destination_planet
    incoming[id] = (incoming[id] or 0) + f.num_ships
  end

  local busy = {} -- planets already tasked this turn

  -- Pass 1: Defend threatened planets
  for _, threatened in ipairs(my_planets) do
    local threat = incoming[threatened.planet_id] or 0
    if threat == 0 then goto next_defend end

    -- Find the closest friendly planet that can spare ships
    local rescuer, rescuer_dist = nil, math.huge
    for _, helper in ipairs(my_planets) do
      if helper.planet_id == threatened.planet_id then goto skip_self end
      if busy[helper.planet_id] then goto skip_self end
      local d = pw:distance(helper, threatened)
      local spare = helper.num_ships - threat - 2
      if spare > 0 and d < rescuer_dist then
        rescuer_dist = d
        rescuer = helper
      end
      ::skip_self::
    end

    if rescuer then
      local send = math.floor(rescuer.num_ships / 2)
      if send > 0 then
        pw:issue_order(rescuer, threatened, send)
        busy[rescuer.planet_id] = true
      end
    end

    ::next_defend::
  end

  -- Pass 2: Attack only when we have overwhelming force (2:1 ratio)
  local targets = pw:not_my_planets()
  for _, src in ipairs(my_planets) do
    if busy[src.planet_id] then goto skip_attack end
    if src.num_ships < 4 then goto skip_attack end

    local best, best_score = nil, -math.huge
    for _, dst in ipairs(targets) do
      local needed = dst.num_ships * 2 + 2
      if src.num_ships >= needed then
        local score = dst.growth_rate * 15 - pw:distance(src, dst)
        if score > best_score then
          best_score = score
          best = dst
        end
      end
    end

    if best then
      pw:issue_order(src, best, math.floor(src.num_ships / 2))
    end

    ::skip_attack::
  end
end
