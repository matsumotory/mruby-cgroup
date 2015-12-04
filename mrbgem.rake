
MRuby::Gem::Specification.new('mruby-cgroup') do |spec|
  spec.license = 'MIT'
  spec.authors = 'MATSUMOTO Ryosuke'
  spec.linker.libraries << ['pthread', 'rt']

  def is_in_linux?
    `grep -q docker /proc/self/cgroup`
    $?.success?
  end

  next unless is_in_linux?

  require 'open3'

  libcgroup_dir = "#{build_dir}/libcgroup"
  libcgroup_build_dir = "#{build_dir}/libcgroup/build"

  task :clean do
    FileUtils.rm_rf [libcgroup_dir]
  end

  def run_command env, command
    STDOUT.sync = true
    puts "build: [exec] #{command}"
    Open3.popen2e(env, command) do |stdin, stdout, thread|
      print stdout.read
      fail "#{command} failed" if thread.value != 0
    end
  end

  FileUtils.mkdir_p build_dir

  if ! File.exists? libcgroup_dir
    Dir.chdir(build_dir) do
      e = {}
      run_command e, 'git clone git://github.com/matsumoto-r/libcgroup.git'
    end
  end

  if ! File.exists? "#{libcgroup_build_dir}/lib/libcgroup.a"
    Dir.chdir libcgroup_dir do
      e = {
        'CC' => "#{spec.build.cc.command} #{spec.build.cc.flags.join(' ')}",
        'CXX' => "#{spec.build.cxx.command} #{spec.build.cxx.flags.join(' ')}",
        'LD' => "#{spec.build.linker.command} #{spec.build.linker.flags.join(' ')}",
        'AR' => spec.build.archiver.command,
        'PREFIX' => libcgroup_build_dir
      }

      run_command e, "./configure --prefix=#{libcgroup_build_dir} --enable-static"
      run_command e, "make"
      run_command e, "make install"
    end
  end

  spec.cc.include_paths << "#{libcgroup_build_dir}/include"
  spec.linker.flags_before_libraries << "#{libcgroup_build_dir}/lib/libcgroup.a"
end
