#![no_std]
#![no_main]

use esp_hal::{
    clock::CpuClock,
    delay::Delay,
    gpio::{Level, Output, OutputConfig},
};
use esp_println::println;
use esp_backtrace as _;
use embedded_hal::delay::DelayNs;

#[panic_handler]
fn panic(info: &core::panic::PanicInfo) -> ! {
    println!("Panic occurred: {:?}", info);
    loop {}
}

#[esp_hal::main]
fn main() -> ! {
    let config = esp_hal::Config::default().with_cpu_clock(CpuClock::max());
    let peripherals = esp_hal::init(config);

    // Create delay instance
    let mut delay = Delay::new();

    // Initialize LED pin (GPIO10)
    let mut led = Output::new(peripherals.GPIO10, Level::Low, OutputConfig::default());

    println!("🚀 ESP32-S3 LED Blinker");
    println!("=====================");
    println!("Hardware initialized successfully!");
    println!("CPU Clock: {:?}", CpuClock::max());
    println!("Chip model: ESP32-S3");

    let mut counter = 0u32;

    loop {
        // Toggle LED
        led.toggle();
        
        // Print status every 10 iterations
        if counter % 10 == 0 {
            println!("💡 LED blinked {} times", counter);
        }
        
        counter += 1;
        delay.delay_ms(500u32); // 500ms delay
    }
}
