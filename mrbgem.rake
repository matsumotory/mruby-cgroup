MRuby::Gem::Specification.new('mruby-cgroup') do |spec|
  spec.license = 'MIT'
  spec.authors = 'MATSUMOTO Ryosuke'
  spec.linker.libraries.concat ['pthread', 'rt']

  def cgroup_available?
    `grep -q cpu /proc/self/cgroup`
    $?.success?
  end

  unless cgroup_available?
    puts "skip libcgroup build"
    next
  end

  require 'open3'

  def libcgroup_dir(b); "#{b.build_dir}/libcgroup"; end
  def libcgroup_build_dir(b); "#{b.build_dir}/libcgroup/build"; end

  task :clean do
    FileUtils.rm_rf [libcgroup_dir(build)]
  end

  def run_command env, command
    STDOUT.sync = true
    puts "EXEC\t[mruby-cgroup] #{command}"
    Open3.popen2e(env, command) do |stdin, stdout, thread|
      print stdout.read
      fail "#{command} failed" if thread.value != 0
    end
  end

  file libcgroup_dir(build) do
    FileUtils.mkdir_p build.build_dir

    unless File.exists? libcgroup_dir(build)
      Dir.chdir(build.build_dir) do
        e = {}
        run_command e, 'git clone git://github.com/matsumoto-r/libcgroup.git'
      end
    end
  end

  file libfile("#{libcgroup_build_dir(build)}/lib/libcgroup") => libcgroup_dir(build) do
    Dir.chdir libcgroup_dir(build) do
      e = {
        'CC' => "#{spec.build.cc.command} #{spec.build.cc.flags.join(' ')}",
        'CXX' => "#{spec.build.cxx.command} #{spec.build.cxx.flags.join(' ')}",
        'LD' => "#{spec.build.linker.command} #{spec.build.linker.flags.join(' ')}",
        'AR' => spec.build.archiver.command,
        'PREFIX' => libcgroup_build_dir(build)
      }

      run_command e, "git checkout ce167ed16147bb68fa1b31633b19de77780d5f2b ."
      run_command e, "autoreconf --force --install"
      run_command e, "./configure --prefix=#{libcgroup_build_dir(build)} --enable-static --disable-shared"
      run_command e, "make"
      run_command e, "make install"
    end
  end

  file libfile("#{build.build_dir}/lib/libmruby") => libfile("#{libcgroup_build_dir(build)}/lib/libcgroup")

  spec.cc.include_paths << "#{libcgroup_build_dir(build)}/include"
  spec.linker.flags_before_libraries << libfile("#{libcgroup_build_dir(build)}/lib/libcgroup")
end
