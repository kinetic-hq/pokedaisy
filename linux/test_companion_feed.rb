# PAD FEED (pc_companion.rb) in a mock RGSS, run by ctest when a ruby is found: the buttons reach Input, and still
# do after a game defines its Input afresh (Awakening, after its language choice). Usage: ruby test_companion_feed.rb pc_companion.rb
# Mock RGSS: mkxp-z's Input first, then a game script defining its own keyboard Input (Awakening).
PADF = File.join(ENV['TEMP'] || '/tmp', 'pd_rewrap_pad.txt')
File.open(PADF, 'wb') { |f| f.write("m0;\n") }
ENV['POKEDAISY_PC_PAD'] = PADF
RESTARTF = File.join(ENV['TEMP'] || '/tmp', 'pd_restart.txt')
File.delete(RESTARTF) if File.exist?(RESTARTF)
ENV['POKEDAISY_PC_RESTART'] = RESTARTF
ENV['POKEDAISY_PC_EXE'] = 'Game.exe'
module Graphics
  @fc = 0
  def self.frame_count; @fc; end
  def self.tick; @fc += 1; end
  def self.frame_rate; 40; end
  def self.update; end
end
module Input
  C = 13; B = 12; A = 11; DOWN = 2; UP = 8; LEFT = 4; RIGHT = 6
  def self.update; end
  def self.press?(b); false; end
  def self.trigger?(b); false; end
  def self.repeat?(b); false; end
  def self.dir4; 0; end
  def self.dir8; 0; end
end
src = File.read(ARGV[0])
# Only the module, none of its hooks into the game at load.
eval(src.sub(/\nPokeDaisyCompanion\.\w+.*\z/m, "\n"), TOPLEVEL_BINDING) rescue (puts "load: #{$!}")
C = PokeDaisyCompanion
def frame; Graphics.tick; Input.update; end
def pad(m); File.open(PADF, 'wb') { |f| f.write("m#{m};\n") }; end
C.feed_setup
ok = true
pad(1); frame
ok &&= Input.trigger?(Input::C) && Input.press?(Input::C)
frame
ok &&= !Input.trigger?(Input::C) && Input.press?(Input::C)
pad(0); frame
puts "native: #{ok}"
# The game's own Input, defined later: new button numbers and methods.
module Input
  remove_const(:C); C = 16
  class << self
    alias game_old_update update
  end
  def self.update; game_old_update; end
  def self.press?(b); false; end
  def self.trigger?(b); false; end
end
pad(1); frame
before = Input.trigger?(Input::C)
C.feed_check
pad(0); frame; pad(1); frame
after = Input.trigger?(Input::C) && Input.press?(16)
frame
held = !Input.trigger?(Input::C) && Input.press?(Input::C)
puts "redefined: before=#{before} after=#{after} held=#{held}"
# A game whose loop runs Input.update more often than the frame count moves (Indigo): one press is one trigger?,
# the cursor moves one row, not one for each update until the next frame.
pad(0); frame
pad(1 << 10) # DOWN (PAD_BUTTONS' 11th)
triggers = 0
3.times { Input.update; triggers += 1 if Input.trigger?(Input::DOWN) && Input.repeat?(Input::DOWN) }
Graphics.tick
3.times { Input.update; triggers += 1 if Input.trigger?(Input::DOWN) }
once = triggers == 1 && Input.press?(Input::DOWN)
pad(0); frame
puts "one press, one trigger: #{once} (#{triggers})"
# The game starting itself again (Indigo's VSync: Restart now?) is noted for PokeDaisy instead of run; anything else runs.
started = system('start Game.exe') == true && File.exist?(RESTARTF)
File.delete(RESTARTF) if File.exist?(RESTARTF)
other = system('ruby -e "exit 0"') == true && !File.exist?(RESTARTF)
puts "restart: #{started} other commands: #{other}"
# ZOOM while walking (Indigo): events start and stop every few frames (a step trigger, a move route on the player).
# The zoom must go on once and stay on, not resize the picture back and forth; a message takes it off at once.
module Graphics
  @w = 512; @h = 384; @resizes = 0
  class << self
    attr_reader :resizes
    def width; @w; end
    def height; @h; end
    def resize_screen(w, h); @w = w; @h = h; @resizes += 1; end
  end
end
class Scene_Map; end
class MockInterp; attr_accessor :running; def running?; @running; end; end
class MockSystem; def map_interpreter; @it ||= MockInterp.new; end; end
class MockTemp; attr_accessor :message_window_showing; end
$scene = Scene_Map.new
$game_system = MockSystem.new
$game_temp = MockTemp.new
clock = [100.0]
(class << C; self; end).send(:define_method, :now) { clock[0] }
C.instance_variable_set(:@zoom, -1) # LT once: 1.25 times the picture
step = lambda { |busy| $game_system.map_interpreter.running = busy; C.apply_zoom; clock[0] += 0.25 }
# Two seconds of quiet map: zoomed once.
8.times { step.call(false) }
zoomedOnce = Graphics.resizes == 1 && Graphics.height == 480 && C.zoom_percent == 80
# Ten seconds of walking with an event flickering on and off: no resize at all.
before = Graphics.resizes
40.times { |i| step.call(i % 3 == 0) }
steady = Graphics.resizes == before && Graphics.height == 480
# A message: the game's own size at once, then back after a second of quiet.
$game_temp.message_window_showing = true
step.call(false)
messageOff = Graphics.height == 384 && C.zoom_percent == 100
$game_temp.message_window_showing = false
step.call(false)
notYet = Graphics.height == 384
6.times { step.call(false) }
back = Graphics.height == 480 && Graphics.resizes == before + 2
puts "zoom: once=#{zoomedOnce} steady=#{steady} message=#{messageOff} wait=#{notYet} back=#{back} (#{Graphics.resizes} resizes)"
# Events that keep running past the hold, again and again: ZOOM turned off for the play, not pumped.
4.times do
  12.times { step.call(true) }
  8.times { step.call(false) }
end
off = C.zoomOff == true && Graphics.height == 384
resizes = Graphics.resizes
8.times { step.call(false) }
stays = Graphics.resizes == resizes
puts "zoom off after #{resizes} resizes: #{off} stays off: #{stays}"
zoomOk = zoomedOnce && steady && messageOff && notYet && back && off && stays
# One mapping: PokeDaisy's ZOOM labels (pc_game.c's PC_ZOOM_PCT) are the companion's steps as a share of the game.
table = File.open(File.join(File.dirname(ARGV[0]), 'pc_game.c'), 'rb') { |f| f.read }[/PC_ZOOM_PCT\[\] = \{([^}]*)\}/, 1].to_s
labels = table.split(',').map { |x| x.to_i }
want = [-2, -1, 0, 1, 2].map { |k| (100.0 / (class << C; self; end)::ZOOM_STEPS[k]).round }
sameTable = labels == want
puts "zoom labels #{labels.inspect} match the companion's steps #{want.inspect}: #{sameTable}"
exit(ok && after && held && once && started && other && zoomOk && sameTable ? 0 : 1)
