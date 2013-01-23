MRuby::Gem::Specification.new('mruby-cgroup') do |spec|
  spec.license = 'MIT'
  spec.authors = 'MATSUMOTO Ryosuke'
  spec.linker.libraries << 'cgroup'
end
