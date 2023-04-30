#! /usr/bin/env node
import cp from 'child_process';
import { Observable } from 'rxjs';
import { SerialPort } from 'serialport';

const { default: pkg } = await import("../package.json", {
  assert: {
    type: "json",
  },
});

const exec = cp.exec;
const task = process.env.npm_config_task;
const project = process.env.npm_config_project_path;
const port = process.env.npm_config_port;
const gateway = process.env.npm_config_gateway;
let macAddress = process.env.npm_config_mac_address;
const deviceName = process.env.npm_config_device_name;
const interval = process.env.npm_config_interval;

if(!task) {
  console.log(`specify --task=<taskname>`);
  process.exit(0);
}
if(task != 'help') {
  if(task == 'ping' || task == 'query' || task == 'calibrateAir' || task == 'calibrateWater' || task == 'updateName' || task == 'updateInterval') {
    if(!gateway || !macAddress) {
      console.log(`specify --gateway=<gateway_url> --mac_address=<esp_mac_address>`);
      process.exit(0);  
    }
    if(task == 'updateName' && !deviceName || task == 'updateInterval' && !interval) {
      if(task == 'updateInterval') {
        console.log(`specify --gateway=<gateway_url> --mac_address=<esp_mac_address> --interval=<interval>`);
      } else {
        console.log(`specify --gateway=<gateway_url> --mac_address=<esp_mac_address> --device_name=<device name>`);
      }
      process.exit(0);
    }
  } else if(task != 'getSerialPorts' && !port) {
    console.log(`specify --port=<serial_port>`);
    process.exit(0);
  } else if(task != 'getSerialPorts' && task != 'erase' && !project) {
    console.log(`specify --project_path=<path_to_project_bin_files>`);
    process.exit(0);
  }  
  if(macAddress) {
    if(macAddress.indexOf(':') > 0 && macAddress.length != 17 || macAddress.indexOf(':') <= 0 && macAddress.length != 12) {
      console.log(`Invalid Mac Address ${macAddress}`);
      process.exit(0);  
    } else if(macAddress.indexOf(':') < 0 && macAddress.length == 12) {
      macAddress = macAddress.replace(/(.{2})/g, '$1:').substring(0,17);
    }
  }  
}

let esptool = {
  getSerialPorts: () => {
    return new Observable((observer) => {
      SerialPort.list().then(function(ports){
        ports.forEach(function(port){
          console.log("Port: ", port);
        });
      });
    });  
  },
  help: () => {
    return new Observable((observer) => {
      console.log(pkg.scripts);
      observer.complete();
    });  
  },
  erase: () => {
    return new Observable((observer) => {
      let arg = `esptool.py --chip esp32 -p ${port} erase_flash`;
      esptool.shell(arg,`done erasing`, `failed to flash`)
      .subscribe(() => {
        observer.next();
        observer.complete();
      });
    });  
  },
  flash: () => {
    return new Observable((observer) => {
      let arg = `esptool.py --chip esp32 -b 460800 -p ${port} --before=default_reset --after=hard_reset write_flash --flash_mode dio --flash_size detect 0x8000 ${project}/partitions.bin 0x1000 ${project}/bootloader.bin 0x10000 ${project}/firmware.bin`;
      esptool.shell(arg,`done flashing ${project}`, `failed to flash ${project}`)
      .subscribe(() => {
        observer.next();
        observer.complete();
      });
    });  
  },
  ping: () => {
    return new Observable((observer) => {
      let arg = `curl ${gateway}/ping\\?host_addr\\=${macAddress}`;
      esptool.shell(arg,`\ndone pinging ${macAddress}\n`, `\nfailed to ping ${macAddress}\n`)
      .subscribe(() => {
        observer.next();
        observer.complete();
      });
    });  
  },
  query: () => {
    return new Observable((observer) => {
      let arg = `curl ${gateway}/query\\?host_addr\\=${macAddress}`;
      esptool.shell(arg,`\ndone querying ${macAddress}\n`, `\nfailed to query ${macAddress}\n`)
      .subscribe(() => {
        observer.next();
        observer.complete();
      });
    });  
  },
  calibrateAir: () => {
    return new Observable((observer) => {
      let arg = `curl ${gateway}/calibrate\\?value\\=air_value\\&host_addr\\=${macAddress}`;
      esptool.shell(arg,`\ndone calibrating air value for ${macAddress}\n`, `\nfailed to calibrate air value for ${macAddress}\n`)
      .subscribe(() => {
        observer.next();
        observer.complete();
      });
    });  
  },
  calibrateWater: () => {
    return new Observable((observer) => {
      let arg = `curl ${gateway}/calibrate\\?value\\=water_value\\&host_addr\\=${macAddress}`;
      esptool.shell(arg,`\ndone calibrating water value for ${macAddress}\n`, `\nfailed to calibrate water value for ${macAddress}\n`)
      .subscribe(() => {
        observer.next();
        observer.complete();
      });
    });  
  },
  updateName: () => {
    return new Observable((observer) => {
      let arg = `curl ${gateway}/update\\?host_addr\\=${macAddress}\\&device_name\\=${deviceName}`;
      esptool.shell(arg,`\ndone updating device name to ${deviceName} for ${macAddress}\n`, `\nfailed to update device name for ${macAddress}\n`)
      .subscribe(() => {
        observer.next();
        observer.complete();
      });
    });  
  },
  updateInterval: () => {
    return new Observable((observer) => {e
      let arg = `curl ${gateway}/update\\?host_addr\\=${macAddress}\\&esp_interval\\=${interval}`;
      esptool.shell(arg,`\ndone updating interval to ${interval} for ${macAddress}\n`, `\nfailed to update interval for ${macAddress}\n`)
      .subscribe(() => {
        observer.next();
        observer.complete();
      });
    });  
  },
  shell: (arg, success='command executed successfully', error='command failed', prnStdout=true, options={maxBuffer: 1024 * 2000}) => {
    return new Observable((observer) => {
      console.log(arg);
      let child = exec(arg, options, (err, stdout, stderr) => {
        if(!err) {
          // console.log(stdout);
          console.log(success);
          observer.next(prnStdout ? stdout : '');
          observer.complete();
        } else {
          console.log(`${error}: ${err}`);
          observer.error(err);
        }
      });
      child.stdout.pipe(process.stdout);
      child.on('data', (data) => {
        console.log('data: ', data)
      })  
    });  
  }  
};

esptool[task]().subscribe(() => {});
