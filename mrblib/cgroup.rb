module Cgroup
  def root_attach root
    if !root.exist?
      raise "root group not found"
    end
    root.attach
  end
  class CPU
    def detach
      root_attach Cgroup::CPU.new "/"
    end
  end
  class CPUSET
    def detach
      root_attach Cgroup::CPUSET.new "/"
    end
  end
  class BLKIO
    def detach
      root_attach Cgroup::BLKIO.new "/"
    end
  end
end
