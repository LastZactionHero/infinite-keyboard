#include <Arduino.h>
#include "driver/i2s.h"
#include <Wire.h>

// --- Keyboard Matrix Definitions ---
const int NUM_ROWS = 6;
const int NUM_COLS = 4;

// Define the GPIO pins for columns (outputs)
const int colPins[NUM_COLS] = {
  37, // COL 1: IO37
  38, // COL 2: IO38
  39, // COL 3: IO39
  40  // COL 4: IO40
};

// Define the GPIO pins for rows (inputs with pull-downs)
const int rowPins[NUM_ROWS] = {
  41, // ROW 1: IO41
  42, // ROW 2: IO42
  44, // ROW 3: RXD0 (GPIO44)
  43, // ROW 4: TXD0 (GPIO43)
  2,  // ROW 5: IO2
  1   // ROW 6: IO1
};

// Array to keep track of the last known state of each key
bool lastKeyState[NUM_ROWS][NUM_COLS];
int activeKeyRow = -1;
int activeKeyCol = -1;

// --- Audio Definitions ---
// I2S Pin Definitions (to PCM5100A DAC)
#define I2S_BCK_PIN   16 // Bit Clock
#define I2S_LRCK_PIN  18 // Left/Right Clock (Word Select)
#define I2S_DIN_PIN   17 // Data Out

// DAC Control Pin (to PCM5100A)
#define PCM_XSMT_PIN  8  // Soft Mute for DAC (HIGH = un-muted)

// I2C Pin Definitions (to TPA6130A2 Amplifier)
#define TPA_SCL_PIN   7  // I2C Clock
#define TPA_SDA_PIN   15 // I2C Data

// TPA6130A2 Amplifier I2C Address
#define TPA6130A2_ADDR 0x60 // Assuming ADDR pin is LOW

// Audio Parameters
#define SAMPLE_RATE         44100
#define BITS_PER_SAMPLE     I2S_BITS_PER_SAMPLE_16BIT
#define NUM_AUDIO_CHANNELS  I2S_CHANNEL_FMT_RIGHT_LEFT // Stereo
#define VOLUME_PERCENTAGE   0.15f  // Reduced volume for digital signal to prevent distortion

// Buffer for I2S data
#define I2S_BUFFER_SIZE 256 // Number of stereo samples (L/R pairs)
int16_t i2s_buffer[I2S_BUFFER_SIZE * 2]; // Buffer for 2 channels

// Sine wave generation
float currentPlayingFrequency = 0.0f; // 0.0f means silence
float phase = 0.0;

// Note frequencies for 2 octaves (24 notes) starting from C4
const float chromaticScale[24] = {
  261.63, 277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00, 466.16, 493.88, // C4 to B4
  523.25, 554.37, 587.33, 622.25, 659.25, 698.46, 739.99, 783.99, 830.61, 880.00, 932.33, 987.77  // C5 to B5
};

// 2D array to map physical key [row][col] to a frequency
float keyToNoteFrequency[NUM_ROWS][NUM_COLS];


// --- TPA6130A2 Amplifier Control ---
void set_tpa6130a2_register(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TPA6130A2_ADDR);
  Wire.write(reg);
  Wire.write(value);
  byte error = Wire.endTransmission();
  if (error != 0) {
    Serial.print("Error writing to TPA6130A2 register 0x");
    Serial.print(reg, HEX);
    Serial.print(": Error code ");
    Serial.println(error);
  }
}

void setup_tpa6130a2() {
  Serial.println("Configuring TPA6130A2 Headphone Amplifier...");
  // Register 1 (0x01): Control Register
  // Enable Left (Bit 7=1), Enable Right (Bit 6=1), Mode=Stereo (Bits 5:4=00), SWS=0 (Active)
  set_tpa6130a2_register(0x01, 0xC0); 
  delay(10); 

  // Register 2 (0x02): Volume and Mute Register
  // Unmute Left (Bit 7=0), Unmute Right (Bit 6=0)
  // Set Volume to 0dB: Volume[5:0] = 011100b (28d) -> Value: 00011100b = 0x1C
  set_tpa6130a2_register(0x02, 0x1C); 
  Serial.println("TPA6130A2 configuration complete.");
}

// --- I2S Setup ---
void setup_i2s() {
  Serial.println("Configuring I2S interface...");
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = BITS_PER_SAMPLE,
    .channel_format = NUM_AUDIO_CHANNELS,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64, 
    .use_apll = true,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK_PIN,
    .ws_io_num = I2S_LRCK_PIN,
    .data_out_num = I2S_DIN_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("I2S driver install failed"); return;
  }
  if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) {
    Serial.println("I2S set pin failed"); return;
  }
  if (i2s_set_clk(I2S_NUM_0, SAMPLE_RATE, BITS_PER_SAMPLE, I2S_CHANNEL_STEREO) != ESP_OK) {
    Serial.println("I2S set clock failed"); return;
  }
  Serial.println("I2S interface configured.");
}

// --- Setup Custom Key Mapping ---
void setupKeyMap() {
  // Initialize all keys to silence (0.0f frequency)
  for (int r_init = 0; r_init < NUM_ROWS; r_init++) {
    for (int c_init = 0; c_init < NUM_COLS; c_init++) {
      keyToNoteFrequency[r_init][c_init] = 0.0f;
    }
  }

  // Populate based on user's mapping (0-indexed rows/cols from user's 1-indexed list)
  // User list: Note - UserRow, UserCol -> (UserRow-1), (UserCol-1) for 0-indexed array
  // Frequencies from chromaticScale: C4=0, C#4=1, ..., B4=11, C5=12, ..., B5=23

  keyToNoteFrequency[0][0] = chromaticScale[0];  // C (C4) - row 1, col 1
  keyToNoteFrequency[1][0] = chromaticScale[1];  // Db
  keyToNoteFrequency[2][0] = chromaticScale[2];  // D (D4) - row 3, col 1
  keyToNoteFrequency[3][0] = chromaticScale[3];  // Eb (D#4) - row 4, col 1
  keyToNoteFrequency[4][0] = chromaticScale[4];  // E (E4) - row 5, col 1
  keyToNoteFrequency[5][0] = chromaticScale[6];  // Gb (F#4) - row 6, col 1
  
  keyToNoteFrequency[0][1] = chromaticScale[7];  // G (G4) - row 2, col 2 (overwrites initial Db if it was here)
  keyToNoteFrequency[1][1] = chromaticScale[7];  // G (G4) - row 2, col 2 (overwrites initial Db if it was here)
  keyToNoteFrequency[2][1] = chromaticScale[8];  // Ab (G#4) - row 3, col 2
  keyToNoteFrequency[3][1] = chromaticScale[9];  // A (A4) - row 4, col 2
  keyToNoteFrequency[4][1] = chromaticScale[10]; // Bb (A#4) - row 5, col 2
  keyToNoteFrequency[5][1] = chromaticScale[11]; // B (B4) - row 6, col 2

  keyToNoteFrequency[0][2] = chromaticScale[18]; // Gb (F#5) - row 1, col 3 (overwrites F4 if it was here)
  keyToNoteFrequency[1][2] = chromaticScale[16]; // E (E5) - row 2, col 3
  keyToNoteFrequency[2][2] = chromaticScale[15]; // Eb (D#5) - row 3, col 3
  keyToNoteFrequency[3][2] = chromaticScale[14]; // D (D5) - row 4, col 3
  keyToNoteFrequency[4][2] = chromaticScale[13]; // Db (C#5) - row 5, col 3
  keyToNoteFrequency[5][2] = chromaticScale[12]; // C (C5) "middle c" - row 6, col 3

  keyToNoteFrequency[0][3] = chromaticScale[17]; // F (F5) - row 1, col 4
  keyToNoteFrequency[1][3] = chromaticScale[19]; // G (G5) - row 2, col 4
  keyToNoteFrequency[2][3] = chromaticScale[20]; // Ab (G#5) - row 3, col 4
  keyToNoteFrequency[3][3] = chromaticScale[21]; // A (A5) - row 4, col 4
  keyToNoteFrequency[4][3] = chromaticScale[22]; // Bb (A#5) - row 5, col 4
  keyToNoteFrequency[5][3] = chromaticScale[23]; // B (B5) - row 6, col 4

  // Handling the collisions based on "last one specified wins":
  // Original user list:
  // Db - row 2, col 2 (keyToNoteFrequency[1][1] = chromaticScale[1])
  // G  - row 2, col 2 (keyToNoteFrequency[1][1] = chromaticScale[7]) -> G4 wins
  // F  - row 1, col 3 (keyToNoteFrequency[0][2] = chromaticScale[5])
  // Gb - row 1, col 3 (keyToNoteFrequency[0][2] = chromaticScale[18]) -> Gb5 wins

  // So the direct assignments above already reflect the "last wins" for the collisions.
  // Notes Db4 (index 1) and F4 (index 5) from chromaticScale are effectively not directly playable
  // with this specific layout due to G4 and Gb5 overwriting their physical key positions.
}


// --- Main Setup ---
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32-S3 Keyboard Synthesizer Initialized");

  // Initialize keyboard column pins as outputs (initially LOW)
  for (int i = 0; i < NUM_COLS; i++) {
    pinMode(colPins[i], OUTPUT);
    digitalWrite(colPins[i], LOW);
  }
  // Initialize keyboard row pins as inputs with pull-downs
  for (int i = 0; i < NUM_ROWS; i++) {
    pinMode(rowPins[i], INPUT_PULLDOWN);
  }
  // Initialize last key states
  for (int r_idx = 0; r_idx < NUM_ROWS; r_idx++) {
    for (int c_idx = 0; c_idx < NUM_COLS; c_idx++) {
      lastKeyState[r_idx][c_idx] = false;
    }
  }

  setupKeyMap(); // Populate the keyToNoteFrequency map

  // Configure DAC Mute Pin
  pinMode(PCM_XSMT_PIN, OUTPUT);
  digitalWrite(PCM_XSMT_PIN, HIGH); // Un-mute the DAC
  Serial.println("PCM5100A XSMT pin set HIGH (un-muted).");

  // Initialize I2C for TPA6130A2
  Wire.begin(TPA_SDA_PIN, TPA_SCL_PIN);
  Serial.println("I2C interface initialized.");
  setup_tpa6130a2();

  // Initialize I2S
  setup_i2s();

  Serial.println("Setup complete. Ready to play!");
}

// --- Main Loop ---
void loop() {
  // --- Keyboard Scanning ---
  for (int c = 0; c < NUM_COLS; c++) { // Iterate through columns
    digitalWrite(colPins[c], HIGH);   // Drive current column HIGH

    for (int r = 0; r < NUM_ROWS; r++) { // Read all rows
      bool currentKeyState = (digitalRead(rowPins[r]) == HIGH);
      
      if (currentKeyState && !lastKeyState[r][c]) { // Key newly pressed
        Serial.print("Key Pressed: ROW "); Serial.print(r + 1);
        Serial.print(", COL "); Serial.print(c + 1);
        
        currentPlayingFrequency = keyToNoteFrequency[r][c];
        if (currentPlayingFrequency > 0.0f) {
            Serial.print(", Freq: "); Serial.println(currentPlayingFrequency);
            activeKeyRow = r;
            activeKeyCol = c;
            phase = 0.0; // Reset phase for new note
        } else {
            Serial.println(" -> No note mapped or error.");
        }
      } else if (!currentKeyState && lastKeyState[r][c]) { // Key newly released
        if (r == activeKeyRow && c == activeKeyCol) { // If the active key was released
          Serial.print("Active Key Released: ROW "); Serial.print(r + 1);
          Serial.print(", COL "); Serial.println(c + 1);
          currentPlayingFrequency = 0.0f; // Stop sound
          activeKeyRow = -1;
          activeKeyCol = -1;
        }
      }
      lastKeyState[r][c] = currentKeyState;
    }
    digitalWrite(colPins[c], LOW); // Set column back to LOW
  }

  // --- Audio Generation & Output ---
  int16_t max_amplitude_val; // Renamed to avoid conflict with a potential global
  float phase_increment_val; // Renamed

  if (currentPlayingFrequency > 0.0f) {
    max_amplitude_val = (int16_t)(32767.0f * VOLUME_PERCENTAGE);
    phase_increment_val = (TWO_PI * currentPlayingFrequency) / SAMPLE_RATE;
  } else {
    max_amplitude_val = 0; // Silence
    phase_increment_val = 0.0f;
  }

  for (int i = 0; i < I2S_BUFFER_SIZE * 2; i += 2) {
    float sample_float = sin(phase) * max_amplitude_val;
    int16_t sample_int = (int16_t)sample_float;

    i2s_buffer[i] = sample_int;     // Left channel
    i2s_buffer[i + 1] = sample_int; // Right channel (mono sound on stereo)

    if (currentPlayingFrequency > 0.0f) {
        phase += phase_increment_val;
        if (phase >= TWO_PI) {
          phase -= TWO_PI;
        }
    } else {
        phase = 0.0f; 
    }
  }

  size_t bytes_written = 0;
  i2s_write(I2S_NUM_0, i2s_buffer, sizeof(i2s_buffer), &bytes_written, portMAX_DELAY);

  delay(5); // Reduced delay for better responsiveness, adjust if needed
}
