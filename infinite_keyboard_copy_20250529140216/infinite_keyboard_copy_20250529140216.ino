#include <Arduino.h>
#include "driver/i2s.h" // For I2S audio output
#include <Wire.h>       // For I2C communication with the amplifier

// --- Keyboard Matrix Definitions ---
const int NUM_ROWS = 6;
const int NUM_COLS = 4;
const int NUM_TOTAL_SWITCHES = NUM_ROWS * NUM_COLS; // 24 switches

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

// --- Switch Enum Definition ---
typedef enum {
  SW_INVALID = 0, // Represents an unmapped or invalid switch
  SW1, SW2, SW3, SW4, SW5, SW6,
  SW7, SW8, SW9, SW10, SW11, SW12,
  SW13, SW14, SW15, SW16, SW17, SW18,
  SW19, SW20, SW21, SW22, SW23, SW24
} SwitchID_t;

// --- Note Enum Definition ---
typedef enum {
  NOTE_NONE = 0, 
  C4_N, Db4_N, D4_N, Eb4_N, E4_N, F4_N, Gb4_N, G4_N, Ab4_N, A4_N, Bb4_N, B4_N,
  C5_N, Db5_N, D5_N, Eb5_N, E5_N, F5_N, Gb5_N, G5_N, Ab5_N, A5_N, Bb5_N, B5_N,
  NOTE_COUNT 
} Note_t;

// --- Audio Definitions ---
#define I2S_BCK_PIN   16 // Bit Clock
#define I2S_LRCK_PIN  18 // Left/Right Clock (Word Select)
#define I2S_DIN_PIN   17 // Data Out

#define PCM_XSMT_PIN  8  // Soft Mute for DAC (HIGH = un-muted)

#define TPA_SCL_PIN   7  // I2C Clock for TPA6130A2 Amplifier
#define TPA_SDA_PIN   15 // I2C Data for TPA6130A2 Amplifier
#define TPA6130A2_ADDR 0x60 // TPA6130A2 I2C Address (ADDR pin LOW)

#define SAMPLE_RATE         44100
#define BITS_PER_SAMPLE     I2S_BITS_PER_SAMPLE_16BIT
#define NUM_AUDIO_CHANNELS  I2S_CHANNEL_FMT_RIGHT_LEFT // Stereo
#define VOLUME_PERCENTAGE   0.15f  // Adjust as needed

#define I2S_BUFFER_SIZE 256 
int16_t i2s_buffer[I2S_BUFFER_SIZE * 2]; 

float currentPlayingFrequency = 0.0f; 
float phase = 0.0;


// --- Keyboard State Variables & Mappings ---
SwitchID_t physicalKeyToSwitchID[NUM_ROWS][NUM_COLS];
bool currentSwitchIsPressed[NUM_TOTAL_SWITCHES + 1];
Note_t switchNoteAssignment[NUM_TOTAL_SWITCHES + 1];
const char* noteEnumToStringMap[NOTE_COUNT];
float noteEnumToFrequencyMap[NOTE_COUNT];


// --- TPA6130A2 Amplifier Control ---
void set_tpa6130a2_register(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TPA6130A2_ADDR);
  Wire.write(reg);
  Wire.write(value);
  byte error = Wire.endTransmission();
  if (error != 0) {
    Serial.print("Error writing to TPA6130A2 register 0x");
    Serial.print(reg, HEX); Serial.print(": Error code "); Serial.println(error);
  }
}

void setup_tpa6130a2() {
  Serial.println("Configuring TPA6130A2 Headphone Amplifier...");
  set_tpa6130a2_register(0x01, 0xC0); // Enable L/R, Stereo
  delay(10); 
  set_tpa6130a2_register(0x02, 0x1C); // Unmute L/R, Volume 0dB
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


// --- Mapping Setup Functions ---
void setupSwitchMapping() {
  Serial.println("Mapping physical keys to SwitchID enums (manual assignment)...");
  for (int r_init = 0; r_init < NUM_ROWS; r_init++) {
    for (int c_init = 0; c_init < NUM_COLS; c_init++) {
      physicalKeyToSwitchID[r_init][c_init] = SW_INVALID;
    }
  }
  physicalKeyToSwitchID[0][0] = SW1; physicalKeyToSwitchID[1][0] = SW2; physicalKeyToSwitchID[2][0] = SW3;
  physicalKeyToSwitchID[3][0] = SW4; physicalKeyToSwitchID[4][0] = SW5; physicalKeyToSwitchID[5][0] = SW6;
  physicalKeyToSwitchID[0][1] = SW7; physicalKeyToSwitchID[1][1] = SW8; physicalKeyToSwitchID[2][1] = SW9;
  physicalKeyToSwitchID[3][1] = SW10; physicalKeyToSwitchID[4][1] = SW11; physicalKeyToSwitchID[5][1] = SW12;
  physicalKeyToSwitchID[0][2] = SW18; physicalKeyToSwitchID[1][2] = SW17; physicalKeyToSwitchID[2][2] = SW16;
  physicalKeyToSwitchID[3][2] = SW15; physicalKeyToSwitchID[4][2] = SW14; physicalKeyToSwitchID[5][2] = SW13;
  physicalKeyToSwitchID[0][3] = SW19; physicalKeyToSwitchID[1][3] = SW20; physicalKeyToSwitchID[2][3] = SW21;
  physicalKeyToSwitchID[3][3] = SW22; physicalKeyToSwitchID[4][3] = SW23; physicalKeyToSwitchID[5][3] = SW24;

  Serial.println("VERIFY: Current physical key to Switch ID mapping:");
  for (int r_verify = 0; r_verify < NUM_ROWS; r_verify++) {
    String rowString = "  Row " + String(r_verify) + ": ";
    for (int c_verify = 0; c_verify < NUM_COLS; c_verify++) {
      SwitchID_t sw = physicalKeyToSwitchID[r_verify][c_verify];
      rowString += "C" + String(c_verify) + "=" + (sw != SW_INVALID ? "SW" + String(static_cast<int>(sw)) : "N/A") + "   ";
    }
    Serial.println(rowString);
  }
}

void initializeNoteStrings() {
  Serial.println("Initializing Note Enum to String map...");
  for(int i=0; i < NOTE_COUNT; ++i) noteEnumToStringMap[i] = "N/A";
  noteEnumToStringMap[C4_N] = "C4"; noteEnumToStringMap[Db4_N] = "Db4"; noteEnumToStringMap[D4_N] = "D4";
  noteEnumToStringMap[Eb4_N] = "Eb4"; noteEnumToStringMap[E4_N] = "E4"; noteEnumToStringMap[F4_N] = "F4";
  noteEnumToStringMap[Gb4_N] = "Gb4"; noteEnumToStringMap[G4_N] = "G4"; noteEnumToStringMap[Ab4_N] = "Ab4";
  noteEnumToStringMap[A4_N] = "A4"; noteEnumToStringMap[Bb4_N] = "Bb4"; noteEnumToStringMap[B4_N] = "B4";
  noteEnumToStringMap[C5_N] = "C5"; noteEnumToStringMap[Db5_N] = "Db5"; noteEnumToStringMap[D5_N] = "D5";
  noteEnumToStringMap[Eb5_N] = "Eb5"; noteEnumToStringMap[E5_N] = "E5"; noteEnumToStringMap[F5_N] = "F5";
  noteEnumToStringMap[Gb5_N] = "Gb5"; noteEnumToStringMap[G5_N] = "G5"; noteEnumToStringMap[Ab5_N] = "Ab5";
  noteEnumToStringMap[A5_N] = "A5"; noteEnumToStringMap[Bb5_N] = "Bb5"; noteEnumToStringMap[B5_N] = "B5";
  
  Serial.println("VERIFY: Note Enum to String mapping:");
  for (int i = 1; i < NOTE_COUNT; i++) {
      Serial.print("  NoteEnumVal "); Serial.print(i); Serial.print(" (");
      Serial.print(noteEnumToStringMap[static_cast<Note_t>(i)]); Serial.println(")");
  }
}

void setupNoteMapping() {
  Serial.println("Mapping SwitchIDs to Note_t enums (manual assignment)...");
  for (int i = 0; i <= NUM_TOTAL_SWITCHES; i++) switchNoteAssignment[i] = NOTE_NONE;
  switchNoteAssignment[static_cast<int>(SW1)] = C4_N; switchNoteAssignment[static_cast<int>(SW2)] = Db4_N;
  switchNoteAssignment[static_cast<int>(SW3)] = D4_N; switchNoteAssignment[static_cast<int>(SW4)] = Eb4_N;
  switchNoteAssignment[static_cast<int>(SW5)] = E4_N; switchNoteAssignment[static_cast<int>(SW6)] = Gb4_N; 
  switchNoteAssignment[static_cast<int>(SW7)] = F4_N; switchNoteAssignment[static_cast<int>(SW8)] = G4_N;
  switchNoteAssignment[static_cast<int>(SW9)] = Ab4_N; switchNoteAssignment[static_cast<int>(SW10)] = A4_N;
  switchNoteAssignment[static_cast<int>(SW11)] = Bb4_N; switchNoteAssignment[static_cast<int>(SW12)] = B4_N;
  switchNoteAssignment[static_cast<int>(SW13)] = C5_N; switchNoteAssignment[static_cast<int>(SW14)] = Db5_N;
  switchNoteAssignment[static_cast<int>(SW15)] = D5_N; switchNoteAssignment[static_cast<int>(SW16)] = Eb5_N;
  switchNoteAssignment[static_cast<int>(SW17)] = E5_N; switchNoteAssignment[static_cast<int>(SW18)] = Gb5_N; 
  switchNoteAssignment[static_cast<int>(SW19)] = F5_N; switchNoteAssignment[static_cast<int>(SW20)] = G5_N;
  switchNoteAssignment[static_cast<int>(SW21)] = Ab5_N; switchNoteAssignment[static_cast<int>(SW22)] = A5_N;
  switchNoteAssignment[static_cast<int>(SW23)] = Bb5_N; switchNoteAssignment[static_cast<int>(SW24)] = B5_N;

  Serial.println("VERIFY: Current Switch ID to Note_t enum mapping:");
  for (int i = 1; i <= NUM_TOTAL_SWITCHES; i++) {
    Serial.print("  SW"); Serial.print(i); Serial.print(" -> NoteEnumVal: "); Serial.print(static_cast<int>(switchNoteAssignment[i]));
    Serial.print(" (String: "); Serial.print(noteEnumToStringMap[switchNoteAssignment[i]]); Serial.println(")");
  }
}

void initializeNoteFrequencies() {
  Serial.println("Initializing Note Enum to Frequency map...");
  for(int i=0; i < NOTE_COUNT; ++i) noteEnumToFrequencyMap[i] = 0.0f;
  noteEnumToFrequencyMap[C4_N]  = 261.63f; noteEnumToFrequencyMap[Db4_N] = 277.18f; noteEnumToFrequencyMap[D4_N]  = 293.66f;
  noteEnumToFrequencyMap[Eb4_N] = 311.13f; noteEnumToFrequencyMap[E4_N]  = 329.63f; noteEnumToFrequencyMap[F4_N]  = 349.23f;
  noteEnumToFrequencyMap[Gb4_N] = 369.99f; noteEnumToFrequencyMap[G4_N]  = 392.00f; noteEnumToFrequencyMap[Ab4_N] = 415.30f;
  noteEnumToFrequencyMap[A4_N]  = 440.00f; noteEnumToFrequencyMap[Bb4_N] = 466.16f; noteEnumToFrequencyMap[B4_N]  = 493.88f;
  noteEnumToFrequencyMap[C5_N]  = 523.25f; noteEnumToFrequencyMap[Db5_N] = 554.37f; noteEnumToFrequencyMap[D5_N]  = 587.33f;
  noteEnumToFrequencyMap[Eb5_N] = 622.25f; noteEnumToFrequencyMap[E5_N]  = 659.25f; noteEnumToFrequencyMap[F5_N]  = 698.46f;
  noteEnumToFrequencyMap[Gb5_N] = 739.99f; noteEnumToFrequencyMap[G5_N]  = 783.99f; noteEnumToFrequencyMap[Ab5_N] = 830.61f;
  noteEnumToFrequencyMap[A5_N]  = 880.00f; noteEnumToFrequencyMap[Bb5_N] = 932.33f; noteEnumToFrequencyMap[B5_N]  = 987.77f;

  Serial.println("VERIFY: Note Enum to Frequency mapping:");
  for (int i = 1; i < NOTE_COUNT; i++) {
      Serial.print("  NoteEnumVal "); Serial.print(i); Serial.print(" (");
      Serial.print(noteEnumToStringMap[static_cast<Note_t>(i)]);
      Serial.print("): Freq "); Serial.println(noteEnumToFrequencyMap[static_cast<Note_t>(i)], 2);
  }
}

// --- Main Setup ---
void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("ESP32-S3 Keyboard Synthesizer Initialized");

  // Initialize keyboard column pins
  for (int i = 0; i < NUM_COLS; i++) {
    pinMode(colPins[i], OUTPUT);
    digitalWrite(colPins[i], LOW);
  }
  // Initialize keyboard row pins
  for (int i = 0; i < NUM_ROWS; i++) {
    pinMode(rowPins[i], INPUT_PULLDOWN);
  }

  // Initialize Mappings
  initializeNoteStrings();     
  initializeNoteFrequencies(); 
  setupSwitchMapping();        
  setupNoteMapping();          

  // Initialize switch states
  for (int i = 0; i <= NUM_TOTAL_SWITCHES; i++) {
    currentSwitchIsPressed[i] = false;
  }

  // Configure DAC Mute Pin
  pinMode(PCM_XSMT_PIN, OUTPUT);
  digitalWrite(PCM_XSMT_PIN, HIGH); // Un-mute the DAC
  Serial.println("PCM5100A XSMT pin set HIGH (un-muted).");

  // Initialize I2C for TPA6130A2 Amplifier
  Wire.begin(TPA_SDA_PIN, TPA_SCL_PIN);
  Serial.println("I2C interface initialized for TPA6130A2.");
  setup_tpa6130a2();

  // Initialize I2S
  setup_i2s();

  Serial.println("Setup complete. Ready to play!");
}

// --- Main Loop ---
void loop() {
  // --- Keyboard Scanning ---
  bool keyStateChangedThisScan = false;
  for (int c = 0; c < NUM_COLS; c++) {
    digitalWrite(colPins[c], HIGH);
    delayMicroseconds(50); 

    for (int r = 0; r < NUM_ROWS; r++) {
      bool isPhysicallyPressed = (digitalRead(rowPins[r]) == HIGH);
      SwitchID_t currentSwitchEnum = physicalKeyToSwitchID[r][c];
      
      if (currentSwitchEnum != SW_INVALID) {
        int switchIntValue = static_cast<int>(currentSwitchEnum);
        bool previousState = currentSwitchIsPressed[switchIntValue];

        if (isPhysicallyPressed && !previousState) { // Newly pressed
          currentSwitchIsPressed[switchIntValue] = true;
          keyStateChangedThisScan = true;
          Serial.print("SW"); Serial.print(switchIntValue); Serial.print(" pressed");
          Note_t assignedNoteEnum = switchNoteAssignment[switchIntValue];
          if (assignedNoteEnum != NOTE_NONE) {
            Serial.print(" (Note: "); Serial.print(noteEnumToStringMap[assignedNoteEnum]);
            Serial.print(", Freq: "); Serial.print(noteEnumToFrequencyMap[assignedNoteEnum], 2); Serial.println(" Hz)");
          } else {
            Serial.println(" (Note: N/A, Freq: N/A)");
          }
        } else if (!isPhysicallyPressed && previousState) { // Newly released
          currentSwitchIsPressed[switchIntValue] = false;
          keyStateChangedThisScan = true;
          // Optional: Print release event
          Serial.print("SW"); Serial.print(switchIntValue); Serial.println(" released");
        }
      }
    }
    digitalWrite(colPins[c], LOW); 
  }

  // --- Determine Highest Note to Play ---
  if (keyStateChangedThisScan) { // Only update frequency if a key was pressed or released
    float highestFrequencyPressed = 0.0f;
    Note_t noteForHighestFreq = NOTE_NONE;

    for (int i = 1; i <= NUM_TOTAL_SWITCHES; i++) {
      if (currentSwitchIsPressed[i]) {
        Note_t note = switchNoteAssignment[i];
        if (note != NOTE_NONE) {
          float freq = noteEnumToFrequencyMap[note];
          if (freq > highestFrequencyPressed) {
            highestFrequencyPressed = freq;
            noteForHighestFreq = note;
          }
        }
      }
    }

    if (currentPlayingFrequency != highestFrequencyPressed) {
      phase = 0.0; // Reset phase if the playing note changes
      currentPlayingFrequency = highestFrequencyPressed;
      if (currentPlayingFrequency > 0.0f) {
         Serial.print("Now Playing: "); Serial.print(noteEnumToStringMap[noteForHighestFreq]);
         Serial.print(" ("); Serial.print(currentPlayingFrequency, 2); Serial.println(" Hz)");
      } else {
         Serial.println("Silence");
      }
    }
  }

  // --- Audio Generation & Output ---
  int16_t max_amplitude_val; 
  float phase_increment_val; 

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
        phase = 0.0f; // Ensure phase is reset if silent, though already handled above
    }
  }

  size_t bytes_written = 0;
  esp_err_t result = i2s_write(I2S_NUM_0, i2s_buffer, sizeof(i2s_buffer), &bytes_written, portMAX_DELAY);
  if (result != ESP_OK) {
    Serial.print("I2S Write Error: "); Serial.println(esp_err_to_name(result));
  }
  // No delay(10) here, as i2s_write with portMAX_DELAY will block.
  // The keyboard scan itself and processing provides some delay.
  // If responsiveness is an issue or CPU usage is too high, a small delay can be added.
  // For example, vTaskDelay(pdMS_TO_TICKS(1)); if using FreeRTOS tasks, or a small delay()
}
