# PokeDaisy's companion for PC fan games (RPG Maker XP + Pokemon Essentials on mkxp-z). GPL-3.0, see ../NOTICE.
#
# PokeDaisy adds this file to the game's preloadScript list (in a mkxp.json of its own, the game's folder left as it
# is). It never changes the game: it reads the party, the place and the battle about twice a second and writes them
# as a small JSON to the path in POKEDAISY_PC_OUT, which PokeDaisy draws in the wings. It reads POKEDAISY_PC_CMD for
# the few things PokeDaisy asks of it (fast forward, instant text, zoom), and POKEDAISY_PC_PAD for the controller's
# buttons (PAD FEED). Every step is wrapped so a game this doesn't know can never crash or slow down because of it: a
# step that fails is left out, and one that keeps failing stops.
#
# Old Essentials ($Trainer, PBSpecies, integer ids), v19 and later ($player, GameData, symbols) and Reborn's own
# ($cache) are all read by asking what the objects answer to, never by version numbers.

module PokeDaisyCompanion
  OUT = ENV['POKEDAISY_PC_OUT']
  CMD = ENV['POKEDAISY_PC_CMD']
  LOG = ENV['POKEDAISY_PC_LOG'] # checks: what failed, a line each
  @logged = 0
  @last = 0.0
  @lastText = nil
  @failures = Hash.new(0)
  @frames = 0
  @battle = nil
  @hooked = false
  @hookTries = 0
  @encMap = nil
  @encList = []
  @cmdStamp = nil
  @ff = 1
  @instantText = false
  @zoom = 0
  @zoomApplied = 0
  @baseSize = nil
  @slow = 0

  class << self
    def note(what, e)
      return unless LOG && @logged < 200
      @logged += 1
      File.open(LOG, 'a') { |f| f.puts "#{what}: #{e.class}: #{e.message} @ #{(e.backtrace || [])[0, 2].join(' < ')}" }
    rescue Exception
      nil
    end

    attr_accessor :battle, :ff, :instantText, :fighting

    def now
      Process.clock_gettime(Process::CLOCK_MONOTONIC)
    rescue StandardError
      Time.now.to_f
    end

    # A step that fails three times is not tried again.
    def safe(name, fallback = nil)
      return fallback if @failures[name] >= 3
      yield
    rescue Exception => e
      raise if e.is_a?(SystemExit) || e.is_a?(Interrupt)
      @failures[name] += 1
      note(name, e)
      fallback
    end

    def tick
      @frames += 1
      return if @frames % 10 != 0
      t = now
      hook_battle unless @hooked
      hook_text unless @textHooked
      hook_zoom unless @zoomHooked
      pad_setup unless @padDone
      read_commands if CMD && t - (@cmdAt || 0) >= 0.25 && (@cmdAt = t)
      apply_zoom
      interval = @slow > 3 ? 2.0 : 0.5
      return if t - @last < interval
      @last = t
      dump
      took = now - t
      @slow = took > 0.03 ? @slow + 1 : [@slow - 1, 0].max
    rescue Exception => e
      raise if e.is_a?(SystemExit) || e.is_a?(Interrupt)
      note(:tick, e)
    end

    # ---- the battle: the class is defined after this file runs, so it's hooked once it exists ----
    def hook_battle
      @hookTries += 1
      @hooked = true if @hookTries > 600
      klass = nil
      klass = ::Battle if defined?(::Battle) && ::Battle.is_a?(Class)
      klass ||= ::PokeBattle_Battle if defined?(::PokeBattle_Battle)
      return unless klass
      chain(klass, :initialize) do |obj, args, orig|
        r = orig.call(*args)
        PokeDaisyCompanion.battle = obj
        r
      end
      # The whole battle runs inside pbStartBattle: its end is the battle's (some games never set in_battle).
      chain(klass, :pbStartBattle) do |obj, args, orig|
        begin
          PokeDaisyCompanion.unzoom
          PokeDaisyCompanion.battle = obj
          PokeDaisyCompanion.fighting = true
          orig.call(*args)
        ensure
          PokeDaisyCompanion.fighting = false
          PokeDaisyCompanion.battle = nil
        end
      end
      @hooked = true
    rescue Exception
      @hooked = true
    end

    # A game's method wrapped: HOOK gets the object, the arguments and the original to call. The old way (an alias)
    # so Ruby 1.8 players (an older mkxp) work as well as Ruby 3 ones.
    def chain(klass, name, &hook)
      orig = :"pokedaisy_#{name.to_s.sub('=', '_set')}"
      return false if klass.method_defined?(orig) || klass.private_method_defined?(orig)
      return false unless klass.method_defined?(name) || klass.private_method_defined?(name)
      klass.send(:alias_method, orig, name)
      klass.send(:define_method, name) do |*args|
        me = self
        hook.call(me, args, lambda { |*a| me.send(orig, *a) })
      end
      klass.send(:ruby2_keywords, name) if klass.respond_to?(:ruby2_keywords, true)
      true
    end

    # THE CONTROLLER. A game on mkxp-z's own Input reads the controller itself (PAD native). Older Essentials games
    # replace that Input with one that asks Windows for the keyboard's keys (GetAsyncKeyState), so a controller does
    # nothing there (PAD keys): PokeDaisy presses the game's own keys for the controller's buttons, the first key the
    # game ties to each of its buttons (PAD_KEYS). Either way PAD_ROLES says what the game does with its buttons (its
    # map scene and its player read for the menu, run and the registered item keys), for the controls under the
    # picture.
    PAD_BUTTONS = %w[C B A X Y Z L R F5 UP DOWN LEFT RIGHT]

    def pad_setup
      @padDone = true
      win32 = Input.respond_to?(:getstate) && Input.respond_to?(:buttonToKey)
      @padMode = win32 ? 'keys' : 'native'
      if win32
        keys = []
        PAD_BUTTONS.each do |b|
          next unless Input.const_defined?(b)
          vk = ((Input.buttonToKey(Input.const_get(b)) rescue nil) || [])[0]
          keys << "#{b}:#{vk}" if vk.is_a?(Integer) && vk > 0 && vk < 256
        end
        @padKeys = keys.join(',')
      end
      @padRoles = pad_roles
      feed_setup # every game: no key is pressed for the controller (keys mode too)
    rescue Exception => e
      note(:pad, e)
    end

    # PAD FEED. On mkxp-z PokeDaisy hides the controller from the game's player and used to press its keys instead,
    # which reach the game only while its window has the foreground, and only as the player's key bindings (F1) say.
    # So PokeDaisy writes the game's buttons held to POKEDAISY_PC_PAD ("m<mask>;", a bit for each of PAD_BUTTONS),
    # and Input answers from it too: read at each Input.update, so press?, trigger?, repeat? and release? keep RGSS's
    # own frame by frame meaning, and dir4 and dir8 follow the pad's directions. The game's own reading (the keyboard)
    # still works alongside. PokeDaisy stops pressing keys once the JSON says pad_feed.
    PAD = ENV['POKEDAISY_PC_PAD']

    def feed_setup
      return if @feedOn || !PAD || !File.exist?(PAD)
      @feedBit = {}
      PAD_BUTTONS.each_with_index do |b, i|
        next unless Input.const_defined?(b)
        v = Input.const_get(b)
        @feedBit[v] = i
        @feedBit[b.to_sym] = i
      end
      return if @feedBit.empty?
      @feedNow = @feedPrev = 0
      @feedHeld = Array.new(PAD_BUTTONS.size, 0)
      Input.singleton_class.class_eval do
        alias_method :pokedaisy_feed_update, :update
        def update(*a)
          r = pokedaisy_feed_update(*a)
          PokeDaisyCompanion.feed_read
          r
        end
        %i[press? trigger? repeat? release?].each do |m|
          orig = :"pokedaisy_feed_#{m.to_s.delete('?')}"
          next unless method_defined?(m)
          alias_method orig, m
          define_method(m) { |*a| PokeDaisyCompanion.feed?(m, a[0]) || send(orig, *a) }
        end
        %i[dir4 dir8].each do |m|
          orig = :"pokedaisy_feed_#{m}"
          next unless method_defined?(m)
          alias_method orig, m
          define_method(m) do |*a|
            d = PokeDaisyCompanion.feed_dir(m == :dir8)
            d == 0 ? send(orig, *a) : d
          end
        end
      end
      @feedOn = true
    rescue Exception => e
      note(:feed, e)
    end

    def feed_read
      t = (File.binread(PAD) rescue nil)
      m = t =~ /\Am(\d+);/ ? $1.to_i : @feedNow
      @feedPrev = @feedNow
      @feedNow = m
      @feedHeld.each_index { |i| @feedHeld[i] = m[i] == 1 ? @feedHeld[i] + 1 : 0 }
    rescue Exception => e
      note(:feedread, e)
    end

    def feed?(m, b)
      return false unless @feedOn && (@feedNow | @feedPrev) != 0
      i = @feedBit[b]
      return false unless i
      now = @feedNow[i] == 1
      before = @feedPrev[i] == 1
      case m
      when :press? then now
      when :trigger? then now && !before
      when :release? then before && !now
      else
        return false unless now
        return true unless before
        # RGSS's repeat: the press, then after about 0.4 s every 0.1 s, at the game's frame rate.
        fr = (Graphics.frame_rate rescue 40).to_i
        fr = 40 if fr <= 0
        start = (fr * 0.4).round
        step = [(fr * 0.1).round, 1].max
        n = @feedHeld[i] - 1
        n >= start && (n - start) % step == 0
      end
    rescue Exception
      false
    end

    def feed_dir(eight)
      return 0 unless @feedOn && @feedNow != 0
      on = lambda { |n| (i = PAD_BUTTONS.index(n)) && @feedNow[i] == 1 }
      v = (on.call('DOWN') ? 1 : 0) - (on.call('UP') ? 1 : 0)
      h = (on.call('RIGHT') ? 1 : 0) - (on.call('LEFT') ? 1 : 0)
      return 0 if v == 0 && h == 0
      return 5 - 3 * v + h if eight
      # dir4: the newest of the two when both are held, as RGSS's own.
      return (v > 0 ? 2 : 8) if h == 0
      return (h > 0 ? 6 : 4) if v == 0
      vi = PAD_BUTTONS.index(v > 0 ? 'DOWN' : 'UP')
      hi = PAD_BUTTONS.index(h > 0 ? 'RIGHT' : 'LEFT')
      @feedHeld[hi] <= @feedHeld[vi] ? (h > 0 ? 6 : 4) : (v > 0 ? 2 : 8)
    rescue Exception
      0
    end

    # The game's scripts' text: its script list, and the folders some games load theirs from.
    def script_texts
      list = []
      ($RGSS_SCRIPTS || []).each do |x|
        t = (Zlib::Inflate.inflate(x[2]) rescue nil)
        list << t if t
      end
      (Dir['Scripts/**/*.rb'] + Dir['Data/Scripts/**/*.rb'] + Dir['Plugins/**/*.rb'])[0, 3000].each do |f|
        t = (File.open(f, 'rb') { |io| io.read } rescue nil)
        list << t if t
      end
      list
    end

    def pad_roles
      letter = {}
      %w[C B A X Y Z L R F5].each do |b|
        v = (Input.const_get(b) rescue nil)
        letter[v] ||= b if v.is_a?(Integer)
      end
      found = {}
      script_texts.each do |t|
        lines = t.split("\n")
        lines.each_with_index do |l, i|
          if !found['MENU'] && l =~ /menu_calling\s*=\s*true|pbCallMenu\b|call_menu\b/
            near = lines[[i - 3, 0].max..i].join(' ')
            found['MENU'] = $1 if near =~ /.*Input\.trigger\?\(\s*Input::(\w+)/
          end
          if !found['ITEMS'] && l =~ /pbUseKeyItem\b|ready_menu|PokemonReadyMenu|keyItemCalling|key_item_calling/
            near = lines[[i - 3, 0].max..i].join(' ')
            found['ITEMS'] = $1 if near =~ /.*Input\.trigger\?\(\s*Input::(\w+)/
          end
          if !found['RUN'] && l =~ /def\s+(pbCanRun\?|can_run\?)\s*$/
            body = (lines[i + 1, 12] || []).join(' ')
            found['RUN'] = $1 if body =~ /Input\.press\?\(\s*Input::(\w+)/
          end
        end
        break if found.size == 3
      end
      roles = { 'C' => ['CONFIRM'], 'B' => ['BACK'] }
      %w[MENU RUN ITEMS].each do |r|
        v = found[r] && (Input.const_get(found[r]) rescue nil)
        b = letter[v]
        (roles[b] ||= []) << r if b
      end
      out = []
      %w[C B A X Y Z L R F5].each { |b| out << "#{b}:#{roles[b].join('/')}" if roles[b] }
      out.join(',')
    rescue Exception => e
      note(:roles, e)
      'C:CONFIRM,B:BACK'
    end

    # INSTANT TEXT: the game's message windows show a whole line at once (Essentials' letter by letter off).
    def hook_text
      @textHooked = true
      return unless defined?(::Window_AdvancedTextPokemon)
      chain(::Window_AdvancedTextPokemon, :letterbyletter=) do |obj, args, orig|
        orig.call(PokeDaisyCompanion.instantText ? false : args[0])
      end
    rescue Exception
      nil
    end

    def in_battle?
      return true if @fighting
      $game_temp && $game_temp.respond_to?(:in_battle) && $game_temp.in_battle
    end

    # ---- names ----
    def species_name(sp, form = 0)
      safe(:sname, sp.to_s) do
        if defined?(GameData::Species) && GameData::Species.respond_to?(:get)
          if form && form != 0 && GameData::Species.respond_to?(:get_species_form)
            GameData::Species.get_species_form(sp, form).name
          else
            GameData::Species.get(sp).name
          end
        elsif respond_to_global?(:getMonName)
          (Object.instance_method(:getMonName).arity == 1 rescue false) ? getMonName(sp) : getMonName(sp, form || 0)
        elsif defined?(PBSpecies) && PBSpecies.respond_to?(:getName)
          PBSpecies.getName(sp)
        else
          sp.to_s
        end
      end
    end

    def respond_to_global?(name)
      Object.private_method_defined?(name) || Object.method_defined?(name)
    end

    # A type as its English id in capitals ("FIRE"): a symbol is that already; an old game's number goes through
    # its PBTypes names.
    def type_id(t)
      return nil if t.nil?
      return t.to_s.upcase if t.is_a?(Symbol) || t.is_a?(String)
      if t.is_a?(Integer)
        # The type's constant ("FIRE") first: getName is in the game's language ("FUEGO" in a Spanish game).
        if defined?(PBTypes) && PBTypes.is_a?(Module)
          @typeNames ||= begin
            h = {}
            PBTypes.constants.each do |c|
              v = (PBTypes.const_get(c) rescue nil)
              h[v] ||= c.to_s.upcase if v.is_a?(Integer) && c.to_s =~ /\A[A-Z]+\z/
            end
            h
          end
          names = @typeNames
          @typeNames = nil if names.empty?
          return names[t] if names[t]
        end
        if defined?(PBTypes) && PBTypes.respond_to?(:getName)
          n = PBTypes.getName(t)
          return n.to_s.upcase if n
        end
        if defined?(GameData::Type)
          return GameData::Type.get(t).id.to_s.upcase
        end
      end
      return t.id.to_s.upcase if t.class.name.to_s =~ /GameData/ && t.respond_to?(:id)
      t.to_s.upcase
    end

    def mon_types(m)
      ts = safe(:mtypes, []) do
        if m.respond_to?(:types)
          m.types
        elsif m.respond_to?(:pbTypes)
          m.pbTypes(true)
        elsif m.respond_to?(:type1)
          [m.type1, m.type2]
        else
          []
        end
      end
      (ts || []).compact.map { |t| type_id(t) }.compact.uniq[0, 2]
    end

    def item_name(it)
      return nil if it.nil? || it == 0 || it == :NONE
      safe(:iname, it.to_s) do
        if defined?(GameData::Item) && GameData::Item.respond_to?(:get)
          GameData::Item.get(it).name
        elsif respond_to_global?(:getItemName)
          getItemName(it)
        elsif defined?(PBItems) && PBItems.respond_to?(:getName)
          PBItems.getName(it)
        else
          it.to_s
        end
      end
    end

    STATUS = { 1 => 'SLP', 2 => 'PSN', 3 => 'BRN', 4 => 'PAR', 5 => 'FRZ',
               :SLEEP => 'SLP', :POISON => 'PSN', :BURN => 'BRN', :PARALYSIS => 'PAR', :FROZEN => 'FRZ' }.freeze

    def status_of(m)
      s = safe(:status) { m.status }
      STATUS[s] || ''
    end

    def egg?(m)
      safe(:egg, false) do
        if m.respond_to?(:egg?) then m.egg?
        elsif m.respond_to?(:isEgg?) then m.isEgg?
        else false
        end
      end
    end

    def shiny?(m)
      safe(:shiny, false) do
        if m.respond_to?(:shiny?) then m.shiny?
        elsif m.respond_to?(:isShiny?) then m.isShiny?
        else false
        end
      end
    end

    # The party icon's file, relative to the game's folder: the game's own answer where it has one, else the usual
    # places by the species' id (Reborn keeps Graphics/Icons/<species>.png, v19+ Graphics/Pokemon/Icons/<ID>.png).
    def icon_of(m)
      f = safe(:icon, nil) do
        g = nil
        if defined?(GameData::Species) && GameData::Species.respond_to?(:icon_filename_from_pokemon)
          g = GameData::Species.icon_filename_from_pokemon(m)
        elsif respond_to_global?(:pbPokemonIconFile)
          g = pbPokemonIconFile(m)
        end
        g.is_a?(String) ? g : nil
      end
      f = with_png(f) if f
      return f if f
      sp = safe(:species) { m.species }
      return nil if sp.nil?
      egg = egg?(m)
      @iconCache ||= {}
      key = [sp, egg]
      return @iconCache[key] if @iconCache.key?(key)
      id = sp.is_a?(Integer) ? format('%03d', sp) : sp.to_s
      names = egg ? ["#{id.downcase}egg", "icon#{id}egg", 'iconEgg', 'Egg'] : [id.downcase, "icon#{id}", id]
      dirs = ['Graphics/Icons', 'Graphics/Pokemon/Icons', 'Graphics/Pictures/Icons']
      found = nil
      dirs.each do |d|
        names.each do |n|
          c = "#{d}/#{n}.png"
          (found = c; break) if File.exist?(c)
        end
        break if found
      end
      @iconCache[key] = found
    end

    def with_png(f)
      return f if f =~ /\.(png|gif|bmp)\z/i && File.exist?(f)
      %w[.png .gif .bmp].each { |e| return f + e if File.exist?(f + e) }
      nil
    end

    def form_of(m)
      safe(:form, 0) { m.respond_to?(:form) ? (m.form || 0) : 0 }
    end

    def mon(m)
      return nil unless m
      egg = egg?(m)
      sp = safe(:species) { m.species }
      h = {
        'species' => egg ? 'Egg' : species_name(sp, form_of(m)),
        'id' => sp.to_s,
        'form' => form_of(m),
        'name' => safe(:name, '') { m.name.to_s },
        'level' => safe(:level, 0) { m.level.to_i },
        'hp' => safe(:hp, 0) { m.hp.to_i },
        'maxhp' => safe(:totalhp, 0) { m.totalhp.to_i },
        'status' => status_of(m),
        'egg' => egg,
        'shiny' => shiny?(m)
      }
      h['types'] = egg ? [] : mon_types(m)
      it = safe(:item) { m.respond_to?(:item_id) ? m.item_id : m.item }
      h['item'] = item_name(it) || ''
      h['icon'] = icon_of(m) || ''
      h
    end

    # ---- the player ----
    def player
      return $player if defined?($player) && $player
      return $Trainer if defined?($Trainer) && $Trainer
      nil
    end

    def badges(p)
      safe(:badges, 0) do
        if p.respond_to?(:badge_count) then p.badge_count
        elsif p.respond_to?(:numbadges) then p.numbadges
        elsif p.respond_to?(:badges) then p.badges.count { |b| b }
        else 0
        end
      end
    end

    def play_seconds
      safe(:time, 0) do
        if defined?($stats) && $stats && $stats.respond_to?(:play_time)
          $stats.play_time.to_i
        else
          (Graphics.frame_count / [Graphics.frame_rate, 1].max).to_i
        end
      end
    end

    def map_name(id)
      safe(:mapname, '') do
        if respond_to_global?(:pbGetMapNameFromId)
          pbGetMapNameFromId(id).to_s
        elsif $game_map.respond_to?(:name)
          $game_map.name.to_s
        else
          ''
        end
      end
    end

    # ---- the wild Pokemon here: whatever shape the game keeps them in, as [species, min, max] ----
    def encounters(id)
      return @encList if @encMap == id
      @encMap = id
      @encList = safe(:enc, []) { read_encounters(id) } || []
    end

    def read_encounters(id)
      rows = []
      add = lambda do |sp, lo, hi|
        next if sp.nil? || sp == 0
        rows << [sp, lo.to_i, (hi || lo).to_i]
      end
      if defined?(GameData::Encounter) && GameData::Encounter.respond_to?(:get)
        ver = ($PokemonGlobal.respond_to?(:encounter_version) ? $PokemonGlobal.encounter_version : 0) rescue 0
        e = GameData::Encounter.get(id, ver) rescue nil
        e ||= GameData::Encounter.get(id, 0) rescue nil
        if e && e.respond_to?(:types)
          e.types.each_value { |list| list.each { |s| add.call(s[1], s[2], s[3]) } }
        end
      elsif defined?($PokemonEncounters) && $PokemonEncounters
        types = $PokemonEncounters.instance_variable_get(:@enctypes) || []
        types.each do |list|
          next unless list
          if list.is_a?(Hash) # Reborn: species => [[chance, min, max], ...]
            list.each { |sp, xs| Array(xs).each { |x| add.call(sp, x[1], x[2]) } }
          else
            list.each { |x| add.call(x[0], x[1], x[2]) if x.is_a?(Array) }
          end
        end
      end
      seen = {}
      rows.each do |sp, lo, hi|
        k = sp.to_s
        if seen[k]
          seen[k][2] = [seen[k][2], lo].min
          seen[k][3] = [seen[k][3], hi].max
        else
          seen[k] = [sp, species_name(sp), lo, hi]
        end
      end
      seen.values[0, 16].map do |sp, name, lo, hi|
        { 'species' => name, 'id' => sp.to_s, 'min' => lo, 'max' => hi, 'types' => species_types(sp) }
      end
    end

    def species_types(sp)
      safe(:stypes, []) do
        if defined?(GameData::Species) && GameData::Species.respond_to?(:get)
          d = GameData::Species.get(sp)
          ts = d.respond_to?(:types) ? d.types : [d.type1, d.type2]
          ts.compact.map { |t| type_id(t) }.uniq[0, 2]
        elsif defined?($cache) && $cache.respond_to?(:pkmn) && $cache.pkmn[sp]
          d = $cache.pkmn[sp]
          [d.respond_to?(:Type1) ? d.Type1 : nil, d.respond_to?(:Type2) ? d.Type2 : nil].compact.map { |t| type_id(t) }.uniq
        elsif respond_to_global?(:pbGetSpeciesData) && defined?(SpeciesType1)
          [pbGetSpeciesData(sp, 0, SpeciesType1), pbGetSpeciesData(sp, 0, SpeciesType2)].compact.map { |t| type_id(t) }.uniq
        else
          []
        end
      end
    end

    # The dex's seen and caught counts: v19+ $player.pokedex, older games' pokedexSeen / pokedexOwned.
    def dex_counts(p)
      safe(:dex, nil) do
        if p.respond_to?(:pokedex) && p.pokedex.respond_to?(:owned_count)
          [p.pokedex.seen_count, p.pokedex.owned_count]
        elsif p.respond_to?(:pokedexOwned)
          [p.pokedexSeen, p.pokedexOwned]
        end
      end
    end

    # ---- the battle's foes ----
    def foes
      b = @battle
      return [] unless b
      list = safe(:battlers, []) { b.respond_to?(:battlers) ? b.battlers : b.instance_variable_get(:@battlers) } || []
      out = []
      list.each_with_index do |bt, i|
        next unless bt && i.odd?
        next if safe(:fainted, false) { bt.respond_to?(:fainted?) ? bt.fainted? : bt.isFainted? }
        sp = safe(:bspecies) { bt.species }
        next if sp.nil? || sp == 0
        h = {
          'species' => species_name(sp, safe(:bform, 0) { bt.form || 0 }),
          'id' => sp.to_s,
          'level' => safe(:blevel, 0) { bt.level.to_i },
          'hp' => safe(:bhp, 0) { bt.hp.to_i },
          'maxhp' => safe(:btotal, 0) { bt.totalhp.to_i },
          'status' => status_of(bt),
          'types' => mon_types(bt)
        }
        pk = safe(:bpoke) { bt.pokemon }
        h['icon'] = pk ? (icon_of(pk) || '') : ''
        out << h
      end
      out
    end

    def foe_team
      b = @battle
      return nil unless b
      safe(:foeteam, nil) do
        party = b.respond_to?(:pbParty) ? b.pbParty(1) : b.instance_variable_get(:@party2)
        trainer = b.respond_to?(:trainerBattle?) ? b.trainerBattle? : (b.respond_to?(:opponent) && b.opponent)
        next nil unless trainer && party
        alive = party.count { |m| m && !egg?(m) && m.hp.to_i > 0 }
        { 'count' => party.compact.size, 'alive' => alive }
      end
    end

    # ---- PokeDaisy's asks: a small file of key=value lines ----
    def read_commands
      st = File.mtime(CMD) rescue nil
      return if st.nil? || st == @cmdStamp
      @cmdStamp = st
      File.read(CMD).each_line do |line|
        k, v = line.strip.split('=', 2)
        next unless k && v
        case k
        when 'ff' then @ff = [[v.to_i, 1].max, 8].min
        when 'instant_text' then @instantText = v == '1'
        when 'zoom' then @zoom = [[v.to_i, -2].max, 2].min
        when 'shape'
          w, h = v.split('x').map { |n| n.to_i }
          @shape = w && h && w > 0 && h > 0 ? [w, h] : nil
        when 'window'
          w, h = v.split('x').map { |n| n.to_i }
          @window = w && h && w > 0 && h > 0 ? [w, h] : nil
        end
      end
    rescue StandardError
      nil
    end

    # ZOOM: the camera, through the game's own screen size. Zoomed out (LT), the game draws a bigger picture, so
    # more of the map shows in the same room; zoomed in (RT), a smaller one. Only on the plain map: a message, an
    # event, a menu or a battle gets the game's own size back first (they're laid out for it), and the zoom
    # returns once the map is quiet again.
    ZOOM_STEPS = { -2 => 1.5, -1 => 1.25, 0 => 1.0, 1 => 0.75, 2 => 0.5 }.freeze

    def zoom_scale
      ZOOM_STEPS[@zoom] || 1.0
    end

    def quiet_map?
      return false unless $scene && $scene.class.to_s == 'Scene_Map'
      return false if in_battle?
      return false if $game_temp && $game_temp.respond_to?(:message_window_showing) && $game_temp.message_window_showing
      return false if $game_temp && $game_temp.respond_to?(:in_menu) && $game_temp.in_menu
      return false if $game_player && $game_player.respond_to?(:move_route_forcing) && $game_player.move_route_forcing
      it = $game_system && $game_system.respond_to?(:map_interpreter) ? $game_system.map_interpreter : nil
      return false if it && it.respond_to?(:running?) && it.running?
      true
    end

    # WIDE VIEW (PokeDaisy's shape= ask): on the plain map the picture takes the shape of PokeDaisy's room for it, so
    # more of the map shows beside; menus and battles keep the game's own shape (PokeDaisy's window fits them in).
    def apply_zoom
      return unless Graphics.respond_to?(:resize_screen)
      quiet = quiet_map?
      want = quiet ? zoom_scale : 1.0
      wide = quiet ? @shape : nil
      return if want == (@zoomShown || 1.0) && wide == @wideShown
      set_screen(want, wide)
    rescue Exception
      @zoomShown = want
      @wideShown = wide
    end

    # The menus and battles: the game's own size while they're open.
    def unzoom
      set_screen(1.0) if (@zoomShown || 1.0) != 1.0 || @wideShown
    rescue Exception
      nil
    end

    def set_screen(k, wide = nil)
      @baseSize ||= [Graphics.width, Graphics.height]
      bw, bh = @baseSize
      h = ((bh * k) / 32).round * 32
      w = wide ? ((h * wide[0] / wide[1].to_f) / 8).round * 8 : ((bw * k) / 32).round * 32
      w = ((bw * k) / 32).round * 32 if w < bw * k
      Graphics.resize_screen(w, h)
      # Some players size their window to the new picture: it gets PokeDaisy's size back at once (no flash).
      Graphics.resize_window(@window[0], @window[1]) if @window && Graphics.respond_to?(:resize_window)
      screen_constants(w, h)
      if $scene.respond_to?(:disposeSpritesets) && $scene.respond_to?(:createSpritesets)
        $scene.disposeSpritesets
        $scene.createSpritesets
      end
      $game_player.center($game_player.x, $game_player.y) if $game_player && $game_player.respond_to?(:center)
      @zoomShown = k
      @wideShown = wide
    end

    # Where a game keeps the screen's size as a constant (v19+ Settings, older DEFAULTSCREENWIDTH), it follows.
    def screen_constants(w, h)
      v = $VERBOSE
      $VERBOSE = nil
      if defined?(::Settings) && ::Settings.const_defined?(:SCREEN_WIDTH)
        ::Settings.send(:remove_const, :SCREEN_WIDTH)
        ::Settings.const_set(:SCREEN_WIDTH, w)
        ::Settings.send(:remove_const, :SCREEN_HEIGHT)
        ::Settings.const_set(:SCREEN_HEIGHT, h)
      end
      if Object.const_defined?(:DEFAULTSCREENWIDTH)
        Object.send(:remove_const, :DEFAULTSCREENWIDTH)
        Object.const_set(:DEFAULTSCREENWIDTH, w)
        Object.send(:remove_const, :DEFAULTSCREENHEIGHT)
        Object.const_set(:DEFAULTSCREENHEIGHT, h)
      end
    ensure
      $VERBOSE = v
    end

    def hook_zoom
      @zoomHooked = true
      return unless defined?(::Scene_Map)
      chain(::Scene_Map, :call_menu) do |obj, args, orig|
        PokeDaisyCompanion.unzoom
        orig.call(*args)
      end
    rescue Exception
      nil
    end

    # ---- the JSON ----
    def esc(s)
      s = s.to_s
      s = s.encode('UTF-8', :invalid => :replace, :undef => :replace) if s.respond_to?(:encode)
      s.gsub(/[\\"\x00-\x1f]/) do |c|
        case c
        when '\\' then '\\\\'
        when '"' then '\\"'
        when "\n" then '\\n'
        else format('\\u%04x', c.unpack('C')[0])
        end
      end
    end

    def json(v)
      case v
      when Hash then '{' + v.map { |k, x| "\"#{esc(k)}\":#{json(x)}" }.join(',') + '}'
      when Array then '[' + v.map { |x| json(x) }.join(',') + ']'
      when String then "\"#{esc(v)}\""
      when Integer, Float then v.to_s
      when true then 'true'
      when false then 'false'
      when nil then 'null'
      else "\"#{esc(v)}\""
      end
    end

    def dump
      return unless OUT
      h = { 'v' => 1, 'scene' => ($scene ? $scene.class.to_s : '') }
      p = player
      if p
        h['player'] = safe(:pname, '') { p.name.to_s }
        party = safe(:party, []) { p.respond_to?(:party) ? p.party : [] } || []
        h['party'] = party.compact[0, 6].map { |m| mon(m) }.compact
        h['badges'] = badges(p)
        h['money'] = safe(:money, -1) { p.money.to_i }
        h['time'] = play_seconds
        dex = dex_counts(p)
        h['seen'], h['caught'] = dex if dex
      end
      h['screen_w'] = safe(:gw, 0) { Graphics.width }
      h['screen_h'] = safe(:gh, 0) { Graphics.height }
      if @padMode
        h['pad'] = @padMode
        h['pad_keys'] = @padKeys if @padKeys
        h['pad_roles'] = @padRoles if @padRoles
        h['pad_feed'] = 1 if @feedOn
      end
      if $game_map
        id = safe(:mapid, 0) { $game_map.map_id }
        h['map_id'] = id
        h['map'] = map_name(id)
        h['wild'] = encounters(id)
      end
      battle = in_battle?
      @battle = nil unless battle
      h['battle'] = battle ? true : false
      if battle
        h['foes'] = foes
        t = foe_team
        h['foe_team'] = t if t
      end
      text = json(h)
      return if text == @lastText
      tmp = OUT + '.tmp'
      File.open(tmp, 'wb') { |f| f.write(text) }
      begin
        File.rename(tmp, OUT)
      rescue StandardError
        File.open(OUT, 'wb') { |f| f.write(text) }
      end
      @lastText = text
    rescue Exception => e
      raise if e.is_a?(SystemExit) || e.is_a?(Interrupt)
      @failures[:dump] += 1
      note(:dump, e)
    end
  end
end

# Every frame passes through Graphics.update: the companion looks in every tenth one. FAST FORWARD skips the
# frame waits of all but every Nth frame.
# ENGLISH FIRST: some games list another language before English in their LANGUAGES setting (a Spanish game with an
# English translation), so a first start picks that one, or the game never asks. A new options object (a first
# start, no save yet) starts on English, and the game's language chooser starts on English, when the game has an
# English entry whose text file is there. The player can still pick another one, and a save keeps that choice: an
# options object read from a save is never made anew. Both are wrapped as the game's scripts define them, since the
# game may ask before its first frame. The game's files are never changed.
module PokeDaisyCompanion
  class << self
    def english_index
      list = nil
      begin
        list = ::Settings::LANGUAGES if defined?(::Settings) && ::Settings.is_a?(Module) &&
                                        ::Settings.const_defined?(:LANGUAGES)
      rescue Exception
        list = nil
      end
      list = ::LANGUAGES if !list.is_a?(Array) && defined?(::LANGUAGES)
      return nil unless list.is_a?(Array)
      list.each_with_index do |l, i|
        next unless l.is_a?(Array) && l[0].to_s =~ /english|ingl.s/i
        return i > 0 && english_file?(l[1]) ? [list, i] : nil
      end
      nil
    rescue Exception
      nil
    end

    def english_file?(f)
      f = f.to_s
      return false if f.empty?
      [f, f + '.dat', "messages_#{f}_core.dat", "messages_#{f}_game.dat"].any? { |n| File.exist?('Data/' + n) }
    end

    def english_options(o)
      hit = english_index
      return unless hit
      o.instance_variable_set(:@language, hit[1]) if (o.instance_variable_get(:@language) || 0) == 0
    rescue Exception
      nil
    end

    def choose_language(obj, orig)
      hit = english_index
      return orig.call unless hit
      names = hit[0].map { |l| l[0] }
      if Kernel.respond_to?(:pbShowCommands)
        Kernel.pbShowCommands(nil, names, hit[1], hit[1])
      else
        obj.send(:pbShowCommands, nil, names, hit[1], hit[1])
      end
    rescue Exception => e
      raise if e.is_a?(SystemExit) || e.is_a?(Interrupt)
      orig.call
    end

    # Wraps NAME of KLASS again each time the game (re)defines it. Each wrapper keeps the definition it wrapped
    # under a name of its own: a game's plugin that aliases the method and then redefines it calls the older
    # wrapper, which must call the older definition, not the newest (that would go round forever).
    def wrap_latest(klass, name, &hook)
      @wrapping ||= false
      return if @wrapping
      @wrapping = true
      @wraps = (@wraps || 0) + 1
      orig = :"pokedaisy_latest_#{name}_#{@wraps}"
      klass.send(:alias_method, orig, name)
      klass.send(:define_method, name) do |*args|
        me = self
        hook.call(me, args, lambda { |*a| me.send(orig, *a) })
      end
      klass.send(:private, name) if name == :initialize
    rescue Exception
      nil
    ensure
      @wrapping = false
    end
  end
end

class Object
  def self.method_added(name)
    super
    if self == ::Object && name == :pbChooseLanguage
      PokeDaisyCompanion.wrap_latest(::Object, :pbChooseLanguage) do |obj, args, orig|
        PokeDaisyCompanion.choose_language(obj, orig)
      end
    end
  end

  def self.inherited(sub)
    super
    return unless sub.name.to_s == 'PokemonSystem'
    def sub.method_added(name)
      super
      return unless name == :initialize
      PokeDaisyCompanion.wrap_latest(self, :initialize) do |obj, args, orig|
        r = orig.call(*args)
        PokeDaisyCompanion.english_options(obj)
        r
      end
    end
  rescue Exception
    nil
  end
end

module Graphics
  class << self
    alias_method :pokedaisy_update, :update unless method_defined?(:pokedaisy_update)
    def update(*args)
      n = PokeDaisyCompanion.ff
      if n > 1 && respond_to?(:frame_count) && frame_count % n != 0 && respond_to?(:frame_count=)
        self.frame_count += 1
        PokeDaisyCompanion.tick
        return
      end
      pokedaisy_update(*args)
      PokeDaisyCompanion.tick
    end
  end
end
