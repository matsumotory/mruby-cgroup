MRuby::Gem::Specification.new('mruby-cgroup') do |spec|
  spec.license = 'MIT'
  spec.authors = 'MATSUMOTO Ryosuke'
  #spec.linker.library_paths << '/usr/local/libcgroup/lib'
  #spec.cc.flags << "-I" + '/usr/local/libcgroup/include'
  spec.linker.libraries << 'cgroup'
end
