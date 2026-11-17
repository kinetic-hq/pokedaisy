# PAD FEED (pc_companion.rb) in a mock RGSS, run by ctest when a ruby is found: the buttons reach Input, and still
# do after a game defines its Input afresh (Awakening, after its language choice). Usage: ruby test_companion_feed.rb pc_companion.rb
# Mock RGSS: mkxp-z's Input first, then a game script defining its own keyboard Input (Awakening).
PADF = File.join(ENV['TEMP'] || '/tmp', 'pd_rewrap_pad.txt')
File.open(PADF, 'wb') { |f| f.write("m0;\n") }
ENV['POKEDAISY_PC_PAD'] = PADF
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
exit(ok && after && held ? 0 : 1)
