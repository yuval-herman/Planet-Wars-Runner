function do_turn(pw)
  local source = nil
  local attacked_planets = {}
  for _, p in ipairs(pw:my_planets()) do
    for _, np in ipairs(pw:not_my_planets()) do
      if attacked_planets[np] then break end
      
      if np.num_ships < p.num_ships then
        pw:debug("Planet has", p.num_ships)
        pw:debug("Sending", p.num_ships-1)
        pw:issue_order(p, np, p.num_ships-1)
        attacked_planets[np] = true
        break
      end
    end
  end
end
