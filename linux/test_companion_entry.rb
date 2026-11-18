# NAME ENTRY (pc_companion.rb) in a mock RGSS, run by ctest when a ruby is found: once the controller has pressed a
# button, a game's keyboard name screen opens as its own letter grid for that one entry, and is the keyboard again
# after. Reborn and Rejuvenation style (one scene, USEKEYBOARD), Essentials v17 to v21 style (PokemonEntryScene2), a
# game with neither, and the keyboard kept while the controller hasn't been used.
# Usage: ruby test_companion_entry.rb pc_companion.rb
PADF = File.join(ENV['TEMP'] || '/tmp', 'pd_entry_pad.txt')
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
eval(src.sub(/\nPokeDaisyCompanion\.\w+.*\z/m, "\n"), TOPLEVEL_BINDING) rescue (puts "load: #{$!}")
C = PokeDaisyCompanion
def frame; Graphics.tick; Input.update; end
def pad(m); File.open(PADF, 'wb') { |f| f.write("m#{m};\n") }; end
ok = true
check = lambda do |name, got, want|
  good = got == want
  puts "#{name}: #{got.inspect}#{good ? '' : " (wanted #{want.inspect})"}"
  ok &&= good
end

# No game classes yet: the hook waits, and nothing breaks.
C.hook_entry
check.call('waits for the game', C.instance_variable_get(:@entryHooked) ? true : false, false)

# Reborn / Rejuvenation: one scene, the grid behind USEKEYBOARD.
class PokemonEntryScene
  USEKEYBOARD = true
  def pbStartScene(*a); @how = USEKEYBOARD ? 'keyboard' : 'grid'; end
  def pbEntry1; 'typed'; end
  def pbEntry2; 'picked'; end
  def pbEntry; USEKEYBOARD ? pbEntry1 : pbEntry2; end
  def pbEndScene; end
end
class PokemonEntry
  def initialize(scene); @scene = scene; end
  def pbStartScreen(helptext, minlength, maxlength, initialText, mode = -1, pokemon = nil)
    @scene.pbStartScene(helptext, minlength, maxlength, initialText, mode, pokemon)
    ret = @scene.pbEntry
    @scene.pbEndScene
    ret
  end
end
def enter; PokemonEntry.new(PokemonEntryScene.new).pbStartScreen('Your name?', 1, 12, '', 1); end
C.hook_entry
check.call('hooked', C.instance_variable_get(:@entryHooked) ? true : false, true)
check.call('keyboard, controller unused', enter, 'typed')
C.feed_setup
pad(1); frame; pad(0); frame
check.call('controller used', C.padUsed ? true : false, true)
check.call('grid (USEKEYBOARD)', enter, 'picked')
check.call('USEKEYBOARD back on', PokemonEntryScene::USEKEYBOARD, true)
check.call('grid again', enter, 'picked')

# A game whose entry fails midway (a reset): the option comes back all the same.
class PokemonEntryScene
  def pbEntry2; raise 'reset'; end
end
begin
  enter
rescue RuntimeError
  nil
end
check.call('USEKEYBOARD back after a reset', PokemonEntryScene::USEKEYBOARD, true)
class PokemonEntryScene
  def pbEntry2; 'picked'; end
end

# Essentials v17 to v21: the second scene is the grid.
class PokemonEntryScene2
  def pbStartScene(*a); end
  def pbEntry; 'grid scene'; end
  def pbEndScene; end
end
screen = PokemonEntry.new(PokemonEntryScene.new)
check.call('grid (PokemonEntryScene2)', screen.pbStartScreen('Your name?', 1, 12, ''), 'grid scene')
check.call('scene put back', screen.instance_variable_get(:@scene).class, PokemonEntryScene)
check.call('grid chosen by the game', PokemonEntry.new(PokemonEntryScene2.new).pbStartScreen('?', 1, 12, ''),
           'grid scene')

# A game with only a keyboard scene and no grid at all: left as it is.
class PokemonEntryScene
  remove_const(:USEKEYBOARD)
  def pbStartScene(*a); end
  def pbEntry; 'typed'; end
end
Object.send(:remove_const, :PokemonEntryScene2)
check.call('no grid: keyboard', enter, 'typed')

exit(ok ? 0 : 1)
