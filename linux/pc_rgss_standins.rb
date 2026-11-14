# GPL-3.0, see ../NOTICE. PokeDaisy's stand-ins for the helper DLLs of games made for the original RPG Maker XP
# player (Pokemon Insurgence), when they run on mkxp-z instead: those DLLs reach into the original player's memory
# and can't work on another one. Each call to them does nothing and answers 0, and the game carries on without
# that extra (Mode 7 maps drawn flat, no GIF pictures, no screenshot DLL).
module PokeDaisyStandIns
  DLLS = %w[mgc_hmode7 rubyscreen gif.dll gif rgss linker].freeze

  def self.stub?(dll)
    d = File.basename(dll.to_s).downcase
    DLLS.any? { |s| d == s || d == "#{s}.dll" || d.start_with?(s) }
  end

  class Call
    def initialize(dll, func)
      @dll = dll
      @func = func
    end

    def call(*args)
      0
    end
    alias Call call
  end
end

# Mode 7 maps (MGC's H-Mode7, drawn by its DLL): drawn as ordinary maps instead. Its switch is turned off once the
# game's scripts have made it.
module PokeDaisyStandIns
  @flat = false
  def self.flatten
    return if @flat
    return unless defined?(::Game_System) && ::Game_System.method_defined?(:hm7)
    @flat = true
    ::Game_System.class_eval do
      def hm7
        false
      end

      def hm7=(v)
        @hm7 = false
      end
    end
  rescue Exception
    @flat = true
  end
end

module Graphics
  class << self
    alias_method :pokedaisy_standin_update, :update unless method_defined?(:pokedaisy_standin_update)
    def update(*args)
      PokeDaisyStandIns.flatten
      pokedaisy_standin_update(*args)
    end
  end
end

if defined?(Win32API)
  class Win32API
    class << self
      alias_method :pokedaisy_standin_new, :new unless method_defined?(:pokedaisy_standin_new)
      def new(dll, func, *rest)
        return PokeDaisyStandIns::Call.new(dll, func) if PokeDaisyStandIns.stub?(dll)
        pokedaisy_standin_new(dll, func, *rest)
      end
    end
  end
end
