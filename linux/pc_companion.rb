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
  LOG = ENV['POKEDAISY_PC_LOG'] # what failed, steps that were slow, the frame stats (PokeDaisy copies it to its log)
  # INPUT PATH (POKEDAISY_PC_INPUT): 'feed' answers Input's press?, trigger?, repeat?... from the pad (every game but
  # the four below); 'source' feeds the pad where the game reads its keys (Input.getstate) so its own Input does the
  # rest, or, on mkxp-z's own Input, answers from edges worked out once per Input.update with a repeat on the clock;
  # 'native' feeds nothing (PokeDaisy lets the game's player read the controller itself). LEAN (POKEDAISY_PC_LEAN):
  # the games that lagged (Indigo, Reminiscencia, Reborn, Rejuvenation) also get fewer lookups in the game while the
  # wings are filled (every game gets the calm ZOOM / WIDE VIEW, see apply_zoom).
  INPUT_PATH = %w[feed source native].include?(ENV['POKEDAISY_PC_INPUT'].to_s) ? ENV['POKEDAISY_PC_INPUT'].to_s : 'feed'
  LEAN = ENV['POKEDAISY_PC_LEAN'] == '1'
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

    # A Ruby 1.8 player has no monotonic clock: asked once, not by an exception at every call.
    MONO = defined?(Process::CLOCK_MONOTONIC) && Process.respond_to?(:clock_gettime)

    def now
      MONO ? Process.clock_gettime(Process::CLOCK_MONOTONIC) : Time.now.to_f
    rescue StandardError
      Time.now.to_f
    end

    # SLOW STEPS: a step of PokeDaisy's that held the game more than 20 ms goes to the log (300 at most), so a game
    # that lags names the step it lags on.
    def timed(name)
      t = now
      r = yield
      ms = ((now - t) * 1000).round
      if ms > 20 && LOG && (@slowLogged || 0) < 300
        @slowLogged = (@slowLogged || 0) + 1
        log_line("slow: #{name} #{ms} ms at frame #{@frames}")
      end
      r
    end

    def log_line(text)
      File.open(LOG, 'a') { |f| f.puts text } if LOG
    rescue Exception
      nil
    end

    # FRAME STATS, every game: each Graphics.update's distance from the last, and how often the game ran
    # Input.update and PokeDaisy's wrappers in between. A line every 30 s (every 5 minutes after the first 10):
    # the median and 95th percentile frame, Input.update calls and wrapped calls per frame, the input mode and path.
    $pokedaisy_updates = 0
    $pokedaisy_calls = 0

    def frame_stat
      return unless LOG
      t = now
      unless @fsStart
        @fsStart = t
        @fsList = []
        @fsLines = 0
        @fsUpMax = 0
        @fsResizes = 0
        $pokedaisy_updates = $pokedaisy_calls = 0
        @fsLast = t
        return
      end
      @fsList << (t - @fsLast)
      @fsLast = t
      up = $pokedaisy_updates - (@fsUpSeen || 0)
      @fsUpSeen = $pokedaisy_updates
      @fsUpMax = up if up > @fsUpMax
      return if t - @fsStart < (@fsLines < 20 ? 30 : 300)
      stats_line(t)
    rescue Exception => e
      note(:stats, e)
    end

    STYLE_NAMES = { :calls => 'every answer wrapped', :lean => 'edges once per update, repeat by the clock',
                    :source => "through the game's getstate" }

    def stats_line(t)
      list = @fsList.sort
      n = list.size
      return if n == 0
      ms = lambda { |x| format('%.1f', x * 1000) }
      fr = (Graphics.frame_rate rescue 0).to_i
      slow = list.count { |x| x > 0.05 }
      log_line("stats: #{format('%.0f', t - @fsStart)} s, #{n} frames (#{format('%.1f', n / (t - @fsStart))}/s, " \
               "game asks #{fr}), frame median #{ms.call(list[n / 2])} ms, p95 #{ms.call(list[(n * 95) / 100])} ms, " \
               "max #{ms.call(list[-1])} ms, over 50 ms #{slow}; Input.update #{format('%.2f', $pokedaisy_updates.to_f / n)}" \
               "/frame (max #{@fsUpMax}), wrapped calls #{format('%.1f', $pokedaisy_calls.to_f / n)}/frame; " \
               "input #{@padMode || '?'}, path #{INPUT_PATH}#{@feedStyle ? " (#{STYLE_NAMES[@feedStyle]})" : ''}" \
               "#{LEAN ? ', lean extras' : ''}, screen resizes #{@fsResizes}")
      @fsLines += 1
      @fsStart = t
      @fsList = []
      @fsUpMax = 0
      @fsResizes = 0
      @fsUpSeen = $pokedaisy_updates = 0
      $pokedaisy_calls = 0
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
      timed(:hook_battle) { hook_battle } unless @hooked
      timed(:hook_text) { hook_text } unless @textHooked
      timed(:hook_zoom) { hook_zoom } unless @zoomHooked
      timed(:pad_setup) { pad_setup } unless @padDone
      timed(:feed_check) { feed_check }
      timed(:commands) { read_commands } if CMD && t - (@cmdAt || 0) >= 0.25 && (@cmdAt = t)
      timed(:zoom) { apply_zoom }
      interval = @slow > 3 ? 2.0 : 0.5
      return if t - @last < interval
      @last = t
      timed(:dump) { dump }
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
      # LEAN: the roles come from reading every one of the game's scripts (thousands of files in Reborn and
      # Rejuvenation): read again only when the game's button numbers changed, not each time it redefines Input.
      sig = PAD_BUTTONS.map { |b| (Input.const_get(b) rescue nil) }
      if !LEAN || sig != @rolesSig || !@padRoles
        @rolesSig = sig
        @padRoles = timed(:pad_roles) { pad_roles }
      end
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
      # native: the game's player reads the pad itself, except a game reading only the keyboard, which can't.
      return if @feedOn || !PAD || !File.exist?(PAD) || (INPUT_PATH == 'native' && @padMode != 'keys')
      @feedNow = @feedPrev = 0
      @feedHeld = Array.new(PAD_BUTTONS.size, 0)
      @feedMine = {}
      @feedGen = 0
      @q = [0, 0, 0, 0] # lean: the buttons pressed, triggered, repeating and released at the last Input.update
      @repAt = Array.new(PAD_BUTTONS.size, 0.0)
      @vkHeld = Array.new(256, false)
      $pokedaisy_vk = nil
      @feedStyle = feed_style
      @feedOn = feed_wrap
    rescue Exception => e
      note(:feed, e)
    end

    # How the pad reaches this game. 'feed': every one of Input's answers (press?, trigger?...) wrapped and answered
    # from the pad, edges per Input.update, repeat counted in updates (every game but the four). 'source' and the
    # game's own keyboard Input (old Essentials: Input.getstate asks Windows for a key): getstate says the pad's keys
    # are down, and the game's own update, trigger?, repeat?, dir4... do the rest at their own cost and timing, as
    # with the keyboard (:source). 'source' on mkxp-z's own Input: the answers are still wrapped, but each is one bit
    # test of edges worked out once per Input.update, and repeat? goes by the clock, 0.4 s then every 0.1 s, however
    # often the game runs Input.update (:lean).
    def feed_style
      return :calls if INPUT_PATH == 'feed'
      return :source if @feedStyle != :lean_fallback && Input.respond_to?(:getstate) && Input.respond_to?(:buttonToKey)
      :lean
    end

    FEED_METHODS = [:update, :press?, :trigger?, :repeat?, :release?, :dir4, :dir8]
    SOURCE_METHODS = [:update, :getstate]
    LEAN_ASK = { :press? => 0, :trigger? => 1, :repeat? => 2, :release? => 3 }

    # Wraps each of Input's methods that isn't PokeDaisy's wrapper now. Run again every few frames (feed_check): a game
    # whose scripts define Input afresh after its first screens (Awakening's own keyboard Input comes after its
    # language choice) replaces the wrappers, and the controller stopped there. Its button numbers (Input::C...) may
    # change with it, so they're read again too. Ruby 1.8 as well (XP games on a borrowed player): no singleton_class,
    # no %i[], no File.binread.
    def feed_wrap
      bits = {}
      PAD_BUTTONS.each_with_index do |b, i|
        next unless Input.const_defined?(b)
        bits[Input.const_get(b)] = i
        bits[b.to_sym] = i
      end
      return false if bits.empty?
      @feedBit = bits
      source_keys if @feedStyle == :source
      sc = (class << Input; self; end)
      changed = false
      (@feedStyle == :source ? SOURCE_METHODS : FEED_METHODS).each do |m|
        next unless sc.method_defined?(m)
        cur = sc.instance_method(m)
        next if @feedMine[m] && @feedMine[m] == cur
        @feedGen += 1
        orig = :"pokedaisy_feed_#{@feedGen}"
        sc.send(:alias_method, orig, m)
        style = @feedStyle
        if m == :update && style == :source
          # The pad first, then the game's own update reads its keys (getstate) with the pad's among them.
          sc.send(:define_method, m) do |*a|
            $pokedaisy_updates += 1 if PokeDaisyCompanion.feed_depth(1) == 1
            begin
              PokeDaisyCompanion.feed_read if PokeDaisyCompanion.feed_outer?
              $pokedaisy_asked = false
              r = send(orig, *a)
              PokeDaisyCompanion.source_check if PokeDaisyCompanion.feed_outer?
            ensure
              PokeDaisyCompanion.feed_depth(-1)
            end
            r
          end
        elsif m == :update
          sc.send(:define_method, m) do |*a|
            $pokedaisy_updates += 1 if PokeDaisyCompanion.feed_depth(1) == 1
            begin
              r = send(orig, *a)
            ensure
              PokeDaisyCompanion.feed_depth(-1)
            end
            PokeDaisyCompanion.feed_read
            r
          end
        elsif m == :getstate
          # Called for each of the 256 keys at every update: globals only, no call into the companion.
          sc.send(:define_method, m) do |*a|
            $pokedaisy_calls += 1
            $pokedaisy_asked = true
            v = $pokedaisy_vk
            (v && a[0].is_a?(Integer) && v[a[0]]) || send(orig, *a)
          end
        elsif m == :dir4 || m == :dir8
          sc.send(:define_method, m) do |*a|
            $pokedaisy_calls += 1
            d = PokeDaisyCompanion.feed_dir(m == :dir8)
            d == 0 ? send(orig, *a) : d
          end
        elsif style == :lean
          k = LEAN_ASK[m]
          sc.send(:define_method, m) do |*a|
            $pokedaisy_calls += 1
            PokeDaisyCompanion.lean?(k, a[0]) || send(orig, *a)
          end
        else
          sc.send(:define_method, m) do |*a|
            $pokedaisy_calls += 1
            PokeDaisyCompanion.feed?(m, a[0]) || send(orig, *a)
          end
        end
        @feedMine[m] = sc.instance_method(m)
        changed = true
      end
      if changed && @feedWrapped
        @padDone = false # the game's Input changed: its mode, keys and roles are read again
        note(:feedwrap, RuntimeError.new("Input redefined, wrapped again"))
      end
      @feedWrapped = true
      true
    end

    def feed_check
      feed_wrap if @feedOn
    rescue Exception => e
      note(:feedcheck, e)
    end

    # Inside a wrapped Input.update: a game's update that calls an earlier one (aliased) reaches two wrappers, and
    # only the outermost reads. The depth after the change.
    def feed_depth(d)
      @feedDepth = (@feedDepth || 0) + d
    end

    def feed_outer?
      @feedDepth == 1
    end

    def feed_read
      # Once for each Input.update the game makes (the outermost wrapper's), as RGSS's own: trigger? is true until the
      # next update. Read once a frame (Graphics.frame_count) instead, a game whose loop runs its Input.update more
      # often than the frame count moves (Indigo, at its own 40 frames a second) saw one press as two or three: the
      # cursor skipped rows.
      return if (@feedDepth || 0) > (@feedStyle == :source ? 1 : 0)
      # One handle for the whole play, read again from its start: opening the file at each read made Windows'
      # antivirus scan it after every press, and the game waited for the scan (lag after each button).
      t = nil
      begin
        @padIO ||= File.open(PAD, 'rb')
        @padIO.seek(0)
        t = @padIO.read(16)
      rescue Exception
        (@padIO.close rescue nil) if @padIO
        @padIO = nil
      end
      m = t =~ /\Am(\d+);/ ? $1.to_i : @feedNow
      @feedPrev = @feedNow
      @feedNow = m
      if @feedStyle == :calls
        @feedHeld.each_index { |i| @feedHeld[i] = m[i] == 1 ? @feedHeld[i] + 1 : 0 }
      elsif (m | @feedPrev) != 0 || @q[2] != 0 || @q[3] != 0
        lean_edges(m, @feedPrev)
      end
      source_vks(m) if @feedStyle == :source && m != @vkMask
    rescue Exception => e
      note(:feedread, e)
    end

    # LEAN: the edges, once per Input.update, as bit masks; repeat? by the clock (RGSS's 0.4 s, then every 0.1 s), so a
    # game running Input.update two or three times a frame repeats no faster, and a stall doesn't fire a burst.
    REPEAT_START = 0.4
    REPEAT_STEP = 0.1

    def lean_edges(m, before)
      trig = m & ~before
      rep = trig
      if m != 0
        t = now
        PAD_BUTTONS.size.times do |i|
          next unless m[i] == 1
          @feedHeld[i] += 1
          if trig[i] == 1
            @repAt[i] = t + REPEAT_START
          elsif t >= @repAt[i]
            rep |= 1 << i
            nxt = @repAt[i] + REPEAT_STEP
            @repAt[i] = nxt > t ? nxt : t + REPEAT_STEP
          end
        end
      end
      PAD_BUTTONS.size.times { |i| @feedHeld[i] = 0 if m[i] == 0 }
      @q[0] = m
      @q[1] = trig
      @q[2] = rep
      @q[3] = before & ~m
    end

    def lean?(k, b)
      q = @q[k]
      return false if q == 0
      i = @feedBit[b]
      i ? q[i] == 1 : false
    end

    # SOURCE: the Windows key code the game ties to each of its buttons (its own buttonToKey, the first key), read
    # again with the wrappers; the keys down for the pad's buttons, kept until the pad changes.
    def source_keys
      @vkFor = PAD_BUTTONS.map do |b|
        vk = Input.const_defined?(b) ? (((Input.buttonToKey(Input.const_get(b)) rescue nil) || [])[0]) : nil
        vk.is_a?(Integer) && vk > 0 && vk < 256 ? vk : nil
      end
      @vkMask = nil
    end

    def source_vks(m)
      @vkMask = m
      @vkHeld.fill(false)
      @vkFor.each_with_index { |vk, i| @vkHeld[vk] = true if vk && m[i] == 1 }
      $pokedaisy_vk = m != 0 ? @vkHeld : nil # what getstate's wrapper reads
    end

    # The game's update didn't ask getstate for any key: it reads its keys some other way, so getstate is no source.
    # Back to the lean answers (wrapped again at once), and the log says so. A few updates that skip it (a game pausing
    # its input while its window is behind) are allowed.
    def source_check
      if !$pokedaisy_asked
        @sourceMissed = (@sourceMissed || 0) + 1
        return if @sourceMissed < 40
        @feedStyle = :lean_fallback
        @feedStyle = feed_style
        @feedMine = {}
        $pokedaisy_vk = nil
        note(:source, RuntimeError.new("the game's Input.update never asks getstate: lean answers instead"))
        feed_wrap
      else
        @sourceMissed = 0
      end
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
      return icon_now(m) unless LEAN
      # LEAN: the game's own answer asked once for each look (an old game resolves a dozen file names for it).
      key = [safe(:species) { m.species }, form_of(m), egg?(m), shiny?(m), safe(:gender, 0) { m.gender }]
      @iconOf ||= {}
      @iconOf.clear if @iconOf.size > 400
      return @iconOf[key] if @iconOf.key?(key)
      @iconOf[key] = icon_now(m)
    end

    def icon_now(m)
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
      return @mapName if LEAN && @mapNameId == id && @mapName # LEAN: once a map
      @mapNameId = id
      @mapName = map_name_now(id)
    end

    def map_name_now(id)
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
    # more of the map shows in the same room; zoomed in (RT), a smaller one. Only on the plain map: a message, a
    # menu, a battle or another scene gets the game's own size back first (they're laid out for it).
    #
    # It's decided with hysteresis, never by a single frame. Walking in a big game starts and stops events all the
    # time (step triggers, a move route on the player, an autorun that ends at once): resizing for each one rebuilt
    # the map's sprites and the picture went small and big while the player walked. So an event or a move route keeps
    # the zoom unless it runs ZOOM_BUSY seconds (a real cutscene), the zoom comes back only after ZOOM_BACK seconds
    # of plain map, and a game whose events still pull the zoom off ZOOM_FLIPS times in a minute gets ZOOM turned off
    # for the play (PokeDaisy says so: zoom_off in the JSON).
    ZOOM_STEPS = { -2 => 1.5, -1 => 1.25, 0 => 1.0, 1 => 0.75, 2 => 0.5 }.freeze
    ZOOM_BACK = 1.0
    ZOOM_BUSY = 2.0
    ZOOM_FLIPS = 3

    def zoom_scale
      ZOOM_STEPS[@zoom] || 1.0
    end

    # Where the game is, for the zoom: :map (the plain map), :busy (an event or a forced move route on the map: they
    # come and go while walking) or :scene (a message, a menu, a battle, another scene: the game's own size at once).
    def zoom_place
      return :scene unless $scene && $scene.class.to_s == 'Scene_Map'
      return :scene if in_battle?
      return :scene if $game_temp && $game_temp.respond_to?(:message_window_showing) && $game_temp.message_window_showing
      return :scene if $game_temp && $game_temp.respond_to?(:in_menu) && $game_temp.in_menu
      return :busy if $game_player && $game_player.respond_to?(:move_route_forcing) && $game_player.move_route_forcing
      it = $game_system && $game_system.respond_to?(:map_interpreter) ? $game_system.map_interpreter : nil
      return :busy if it && it.respond_to?(:running?) && it.running?
      :map
    end

    def quiet_map?
      zoom_place == :map
    end

    attr_reader :zoomOff

    def zoomed?
      (@zoomShown || 1.0) != 1.0 || !@wideShown.nil?
    end

    # WIDE VIEW (PokeDaisy's shape= ask): on the plain map the picture takes the shape of PokeDaisy's room for it, so
    # more of the map shows beside; menus and battles keep the game's own shape (PokeDaisy's window fits them in).
    def apply_zoom
      return unless Graphics.respond_to?(:resize_screen)
      t = now
      place = zoom_place
      @busySince = place == :busy ? (@busySince || t) : nil
      @quietSince = place == :map ? (@quietSince || t) : nil
      want = zoom_scale
      wide = @shape
      on = !@zoomOff && (want != 1.0 || wide)
      if on && zoomed?
        # Zoomed: kept on the map and through a short event; a scene, or an event that lasts, takes it off.
        on = place == :map || (place == :busy && t - @busySince < ZOOM_BUSY)
        zoom_flip(t) if !on && place == :busy
      elsif on
        on = place == :map && t - @quietSince >= ZOOM_BACK
      end
      want, wide = 1.0, nil unless on
      return if want == (@zoomShown || 1.0) && wide == @wideShown
      set_screen(want, wide)
    rescue Exception
      @zoomShown = want
      @wideShown = wide
    end

    # An event that took the zoom off: counted, and the zoom given up for the play if they keep coming.
    def zoom_flip(t)
      @zoomFlips = (@zoomFlips || []).select { |x| t - x < 60.0 }
      @zoomFlips << t
      return if @zoomFlips.size < ZOOM_FLIPS
      @zoomOff = true
      note(:zoom_off, RuntimeError.new("events took the zoom off #{@zoomFlips.size} times in a minute"))
    end

    # ZOOM as it's shown now, a share of the game's own size (the zoomed out picture's things are smaller: 67%).
    def zoom_percent
      return 100 unless zoomed? && @baseSize
      h = Graphics.height.to_i
      h > 0 ? (100.0 * @baseSize[1] / h).round : 100
    end

    # The menus and battles: the game's own size while they're open.
    def unzoom
      set_screen(1.0) if zoomed?
      @quietSince = nil
    rescue Exception
      nil
    end

    def set_screen(k, wide = nil)
      @baseSize = [Graphics.width, Graphics.height] unless @baseSize && zoomed?
      bw, bh = @baseSize
      # The game's own size exactly at 100% (rounded to 32, a 360 tall game would come back 352).
      h = k == 1.0 ? bh : ((bh * k) / 32).round * 32
      w = wide ? ((h * wide[0] / wide[1].to_f) / 8).round * 8 : (k == 1.0 ? bw : ((bw * k) / 32).round * 32)
      w = (k == 1.0 ? bw : ((bw * k) / 32).round * 32) if w < bw * k
      Graphics.resize_screen(w, h)
      @fsResizes = (@fsResizes || 0) + 1
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
      # The game's own size, not the zoomed one: PokeDaisy lays its window out by its shape, and a zoomed size
      # rounded to 32 has another shape (the window changed size each time the zoom came and went).
      own = zoomed? && @baseSize ? @baseSize : nil
      h['screen_w'] = safe(:gw, 0) { own ? own[0] : Graphics.width }
      h['screen_h'] = safe(:gh, 0) { own ? own[1] : Graphics.height }
      h['zoom_pct'] = safe(:zoompct, 100) { zoom_percent }
      h['zoom_off'] = 1 if @zoomOff
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
      PokeDaisyCompanion.frame_stat
      PokeDaisyCompanion.tick
    end
  end
end

# RESTART. A game's own option that needs a restart (Indigo's VSync: "Restart now?") starts its exe again itself
# (system, spawn, exec or mkxp-z's System.launch with "Game.exe") and closes. Under PokeDaisy that failed ("Windows
# cannot find 'Game.exe'") and the game was gone; started, it would have run outside PokeDaisy anyway (no controller,
# no wings). So that one launch isn't made: POKEDAISY_PC_RESTART is written instead, and PokeDaisy starts the game
# again in its place once this one has closed. Every other command runs as it always did.
module PokeDaisyCompanion
  RESTART = ENV['POKEDAISY_PC_RESTART']
  EXE = ENV['POKEDAISY_PC_EXE']

  class << self
    # True when ARGS (a command line, or a program and its arguments) start the game's own exe.
    def self_launch?(args)
      words = []
      args.each do |a|
        a = a[0] if a.is_a?(Array) # spawn's [program, argv0]
        words << a if a.is_a?(String)
      end
      line = words.join(' ').downcase
      return false if line.empty? || line.include?('://')
      names = ['game']
      names << File.basename(EXE, '.*').downcase if EXE && !EXE.empty?
      line.split(/["'\s\\\/]+/).any? { |w| names.include?(w.sub(/\.exe\z/, '')) }
    rescue Exception
      false
    end

    # Notes the restart for PokeDaisy. True when it was noted (the launch is then left out).
    def restart_instead(args)
      return false unless RESTART && self_launch?(args)
      File.open(RESTART, 'wb') { |f| f.write("restart\n") }
      note(:restart, RuntimeError.new("the game asked to start again: #{args.inspect[0, 120]}"))
      true
    rescue Exception
      false
    end
  end
end

module Kernel
  [:system, :spawn, :exec].each do |m|
    next unless private_method_defined?(m) || method_defined?(m)
    orig = :"pokedaisy_restart_#{m}"
    next if private_method_defined?(orig) || method_defined?(orig)
    alias_method orig, m
    define_method(m) do |*a|
      if PokeDaisyCompanion.restart_instead(a)
        exit if m == :exec # exec never returns: the game closes, as it would have
        m == :system ? true : 0
      else
        send(orig, *a)
      end
    end
    send(:ruby2_keywords, m) if respond_to?(:ruby2_keywords, true) # options (exception: true) pass through as keywords
    private m
    module_function m rescue nil
  end
end

if defined?(Process) && Process.respond_to?(:spawn)
  class << Process
    alias_method :pokedaisy_restart_spawn, :spawn unless method_defined?(:pokedaisy_restart_spawn)
    def spawn(*a)
      PokeDaisyCompanion.restart_instead(a) ? 0 : pokedaisy_restart_spawn(*a)
    end
    send(:ruby2_keywords, :spawn) if respond_to?(:ruby2_keywords, true)
  end
end

if defined?(System) && System.respond_to?(:launch)
  class << System
    alias_method :pokedaisy_restart_launch, :launch unless method_defined?(:pokedaisy_restart_launch)
    def launch(*a)
      PokeDaisyCompanion.restart_instead(a) ? true : pokedaisy_restart_launch(*a)
    end
  end
end
