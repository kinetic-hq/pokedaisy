# The companion's input paths (pc_companion.rb, POKEDAISY_PC_INPUT) measured in a mock RGSS loop, run by ctest
# (companion-bench) when a ruby is found. Usage: ruby bench_companion_input.rb pc_companion.rb [--full]
#
# Two models of a game's Input:
# - native: mkxp-z's own Input (in the real player it is C; here a small Ruby stand-in, which the baseline subtracts),
#   repeat counted in updates as RGSS does (0.4 s, then every 0.1 s, at the game's 40 frames a second).
# - keys: old Essentials' keyboard Input (Indigo, Reminiscencia and their kind): update asks getstate for each of the
#   256 keys, press? / trigger? / repeat? go through buttonToKey in Ruby, repeat after 20 updates then every other one.
# The game's loop runs Input.update 1 to 3 times a frame, a menu reading repeat?(DOWN) after each, and EXTRA calls of
# press? a frame (a big game polling its hotkeys and its own scripts' input).
#
# BEHAVIOUR (a simulated clock at 40 frames a second): the rows a menu moves for a 150 ms tap and a 1 s hold of DOWN,
# by the pad through each path and by the keyboard (what the game does by itself). TIMING (the real clock): the time a
# frame takes with each path, less the same loop with the companion feeding nothing.
#
# Each case runs in a ruby of its own (Input is one module per process): this file runs itself with "case ...".

require 'rbconfig'

PATHS = %w[feed source]

def run_case(companion, *args)
  out = IO.popen([RbConfig.ruby, __FILE__, companion, 'case', *args.map { |a| a.to_s }]) { |io| io.read }
  raise "case #{args.join(' ')} failed: #{out}" unless $?.success?
  out.strip
end

if ARGV[1] == 'case'
  companion, _, flavour, path, ups, extra, what = ARGV
  companion = File.expand_path(companion)
  $flavour = flavour
  ups = ups.to_i
  extra = extra.to_i
  require 'tmpdir'
  dir = Dir.mktmpdir('pd_bench')
  Dir.chdir(dir) # the companion looks for the game's scripts in its folder: none here
  padf = File.join(dir, 'pad.txt')
  File.open(padf, 'wb') { |f| f.write("m00000;\n") }
  ENV['POKEDAISY_PC_PAD'] = padf
  ENV['POKEDAISY_PC_INPUT'] = %w[none keyboard].include?(path) ? 'native' : path # native: the companion feeds nothing
  ENV['POKEDAISY_PC_LEAN'] = path == 'feed' ? '0' : '1'
  ENV['POKEDAISY_PC_LOG'] = File.join(dir, 'companion.log')
  $sim = nil # the simulated clock, or nil for the real one
  $kb = {}   # BEHAVIOUR's keyboard: the game's buttons held on it
  module Graphics
    @fc = 0
    def self.frame_count; @fc; end
    def self.frame_count=(v); @fc = v; end
    def self.frame_rate; 40; end
    def self.update; @fc += 1; end
    def self.width; 512; end
    def self.height; 384; end
  end
  if flavour == 'native'
    module Input
      DOWN = 2; LEFT = 4; RIGHT = 6; UP = 8; A = 11; B = 12; C = 13; X = 14; Y = 15; Z = 16; L = 17; R = 18
      ALL = [DOWN, LEFT, RIGHT, UP, A, B, C, X, Y, Z, L, R]
      @count = Hash.new(0)
      def self.update
        ALL.each { |b| @count[b] = $kb[b] ? @count[b] + 1 : 0 }
      end
      def self.press?(b); @count[b] > 0; end
      def self.trigger?(b); @count[b] == 1; end
      def self.release?(b); false; end
      def self.repeat?(b)
        n = @count[b] - 1
        n == 0 || (n >= 16 && (n - 16) % 4 == 0)
      end
      def self.dir4; press?(DOWN) ? 2 : press?(UP) ? 8 : press?(LEFT) ? 4 : press?(RIGHT) ? 6 : 0; end
      def self.dir8; dir4; end
    end
  else
    module Input
      DOWN = 2; LEFT = 4; RIGHT = 6; UP = 8; A = 11; B = 12; C = 13; X = 14; Y = 15; Z = 16; L = 17; R = 18; F5 = 25
      KEYS = { DOWN => [0x28], LEFT => [0x25], RIGHT => [0x27], UP => [0x26], A => [0x10], B => [0x1B, 0x58],
               C => [0x0D, 0x20, 0x43], X => [0x41], Y => [0x53], Z => [0x44], L => [0x51], R => [0x57], F5 => [0x74] }
      GAS = Object.new # Win32API.new('user32', 'GetAsyncKeyState', 'i', 'i')
      def GAS.call(k)
        KEYS.each { |b, ks| return -32768 if $kb[b] && ks[0] == k }
        0
      end
      @keystate = Array.new(256, 0)
      def self.getstate(k); (GAS.call(k) & 0x8000) > 0; end
      def self.buttonToKey(b); KEYS[b] || []; end
      if $flavour == 'keys-direct' # an update that asks Windows itself, never through getstate
        def self.update
          256.times { |i| @keystate[i] = (GAS.call(i) & 0x8000) > 0 ? @keystate[i] + 1 : 0 }
        end
      else
        def self.update
          256.times { |i| @keystate[i] = getstate(i) ? @keystate[i] + 1 : 0 }
        end
      end
      def self.pressex?(k); @keystate[k] > 0; end
      def self.triggerex?(k); @keystate[k] == 1; end
      def self.repeatex?(k); @keystate[k] == 1 || (@keystate[k] > 20 && (@keystate[k] & 1) == 0); end
      def self.releaseex?(k); false; end
      def self.press?(b); buttonToKey(b).each { |k| return true if pressex?(k) }; false; end
      def self.trigger?(b); buttonToKey(b).each { |k| return true if triggerex?(k) }; false; end
      def self.repeat?(b); buttonToKey(b).each { |k| return true if repeatex?(k) }; false; end
      def self.release?(b); false; end
      def self.dir4; press?(DOWN) ? 2 : press?(UP) ? 8 : press?(LEFT) ? 4 : press?(RIGHT) ? 6 : 0; end
      def self.dir8; dir4; end
    end
  end
  eval(File.read(companion), TOPLEVEL_BINDING, companion)
  class << PokeDaisyCompanion
    alias_method :bench_real_now, :now
    def now; $sim || bench_real_now; end
  end
  pad = File.open(padf, 'r+b')
  set_pad = lambda do |down|
    if path == 'keyboard'
      $kb = down ? { Input::DOWN => true } : {}
    else
      pad.seek(0)
      pad.write(format("m%05u;\n", down ? 1 << 10 : 0)) # DOWN, PAD_BUTTONS' 11th
      pad.flush
    end
  end
  hot = [Input::A, Input::X, Input::Y, Input::Z, Input::L, Input::R, Input::C, Input::B]
  moves = 0
  frame = lambda do
    ups.times do
      Input.update
      moves += 1 if Input.repeat?(Input::DOWN)
      Input.trigger?(Input::C)
      Input.trigger?(Input::B)
    end
    i = 0
    while i < extra
      Input.press?(hot[i & 7])
      i += 1
    end
    Graphics.update
  end
  if what == 'timing'
    n = 600
    t0 = PokeDaisyCompanion.bench_real_now
    n.times do |f|
      set_pad.call(true) if f == 200
      set_pad.call(false) if f == 500
      frame.call
    end
    puts format('%.1f', (PokeDaisyCompanion.bench_real_now - t0) * 1e6 / n)
  else
    # 40 frames a second; DOWN from 0.5 s for 150 ms (tap) or 1 s (hold). keys-direct: from 3 s, once the source path
    # has found that the game's update never asks getstate and gone back to the lean answers.
    hold = what == 'tap' ? 0.15 : 1.0
    from = flavour == 'keys-direct' ? 3.0 : 0.5
    $sim = 0.0
    (from * 40 + 60).to_i.times do |f|
      $sim = f * 0.025
      set_pad.call($sim >= from && $sim < from + hold - 1e-9)
      frame.call
    end
    puts moves
  end
  exit 0
end

companion = File.expand_path(ARGV[0] || '') ; ARGV[0] or abort 'usage: ruby bench_companion_input.rb pc_companion.rb [--full]'
full = ARGV.include?('--full')
ok = true

puts 'BEHAVIOUR: rows moved by DOWN (40 fps; a menu reads repeat? after each Input.update)'
puts format('%-11s %-4s %-9s %5s %5s', 'input', 'ups', 'path', 'tap', 'hold')
%w[native keys keys-direct].each do |flavour|
  (1..3).each do |ups|
    kb = nil
    (['keyboard'] + PATHS).each do |path|
      tap = run_case(companion, flavour, path, ups, 0, 'tap').to_i
      hold = run_case(companion, flavour, path, ups, 0, 'hold').to_i
      kb ||= [tap, hold]
      puts format('%-11s %-4d %-9s %5d %5d', flavour, ups, path, tap, hold)
      next unless path == 'source'
      # The new path: one row a tap; a hold repeats by the clock on mkxp-z's Input (6 or 7 rows a second, however
      # many updates a frame) and exactly as the game's own keyboard Input does on an old Essentials one.
      good = tap == 1 && (flavour == 'keys' ? [tap, hold] == kb : hold >= 6 && hold <= 8)
      # keys-direct: the source path's own check falls back to the lean answers, which then behave as on mkxp-z's.
      puts "  ^ unexpected for #{flavour} through source" unless good
      ok &&= good
    end
  end
end

puts
puts 'TIMING: microseconds a frame (the companion feeding nothing subtracted), DOWN held for half the frames'
puts format('%-7s %-4s %-6s %10s %10s', 'input', 'ups', 'extra', 'feed', 'source')
best = lambda do |*a|
  runs = (1..(full ? 5 : 2)).map { run_case(companion, *a).to_f }
  runs.min
end
%w[native keys].each do |flavour|
  [1, 3].each do |ups|
    (full ? [0, 500, 2000] : [0, 2000]).each do |extra|
      base = best.call(flavour, 'none', ups, extra, 'timing')
      cost = PATHS.map { |p| best.call(flavour, p, ups, extra, 'timing') - base }
      puts format('%-7s %-4d %-6d %10.1f %10.1f   (no feed: %.1f us a frame)', flavour, ups, extra, cost[0], cost[1], base)
    end
  end
end
puts
puts(ok ? 'companion-bench: ok' : 'companion-bench: FAILED')
exit(ok ? 0 : 1)
