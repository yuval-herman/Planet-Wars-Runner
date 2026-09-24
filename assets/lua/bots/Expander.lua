-- Expander: a patient empire-builder that grows before it fights.
-- Each turn it finds the highest-growth neutral planet it can afford to take
-- and sends the minimum ships needed to capture it.
-- Once no neutrals remain, it turns its fleets against the enemy.

function do_turn(pw)
  local my_planets = pw:my_planets()
  if #my_planets == 0 then return end

  -- Find the richest neutral planet we can reach affordably
  local neutrals  = pw:neutral_planets()
  local enemies   = pw:enemy_planets()

  -- Pick the single most valuable neutral target
  local function best_neutral_target()
    local best, best_score = nil, -math.huge
    for _, dst in ipairs(neutrals) do
      -- Score: growth rate minus cost (ships needed + 1)
      local score = dst.growth_rate * 20 - (dst.num_ships + 1)
      if score > best_score then
        best_score = score
        best = dst
      end
    end
    return best
  end

  local target = best_neutral_target()

  -- Fall back to enemy planets when neutrals are gone
  if not target then
    local best_score = -math.huge
    for _, dst in ipairs(enemies) do
      local score = dst.growth_rate * 10 - dst.num_ships
      if score > best_score then
        best_score = score
        target = dst
      end
    end
  end

  if not target then return end

  -- Send ships from the closest planet that has enough to spare
  local src, src_dist = nil, math.huge
  for _, p in ipairs(my_planets) do
    local d = pw:distance(p, target)
    if d < src_dist and p.num_ships > target.num_ships + 2 then
      src_dist = d
      src = p
    end
  end

  if src then
    local send = target.num_ships + 1
    pw:issue_order(src, target, send)
  end
end
