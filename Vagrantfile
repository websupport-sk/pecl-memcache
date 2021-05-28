# -*- mode: ruby -*-
# vi: set ft=ruby :

VAGRANTFILE_API_VERSION = '2'

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|
    config.vm.box = 'ubuntu/bionic64'

    config.vm.provider :virtualbox do |vb|
        vb.name = 'ext-memcache-dev'
        vb.memory = 1024
        vb.cpus = 2
    end

    config.vm.provision 'docker'

end
