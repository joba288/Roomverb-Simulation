#include "raylib.h"
#include "raymath.h"


#define MAX_SOURCES 4
#define SAMPLE_RATE 44100
#define MAX_TIME 2
#define MAX_IR_SAMPLES (MAX_TIME * SAMPLE_RATE)

typedef struct
{
	float* left;
	float* right;
	int sampleCount;
	int sampleRate;
} AudioBuffer;

typedef struct
{
	float* left;
	float* right;
	int bands;
	int sampleCount;
	int sampleRate;
} IRBuffer;

typedef struct
{
	Vector3 pos;

	AudioBuffer* source_audio;
	IRBuffer* ir_buffer;
	AudioBuffer* convoluted;

	const char* source_filepath;
	const char* ir_filepath;
	const char* convoluted_filepath;

} AudioSource;



// function checklist
void init_audio_buffer(AudioBuffer* buffer, int samples, int rate);
void free_audio_buffer(AudioBuffer* buffer);
void init_ir_buffer(IRBuffer* ir, int bands, int samples, int rate);
void free_ir_buffer(IRBuffer* ir);
void normalise_ir_buffer(IRBuffer* ir); // squish down frequencies also

int input_wav_stereo(const char* filepath, AudioBuffer* in);
int output_wav_stereo(const char* filepath, AudioBuffer* out);
void ir_buffer_to_audio_buffer(IRBuffer* ir, AudioBuffer* dest);
void audio_buffer_to_ir_buffer(AudioBuffer* buffer, IRBuffer* dest);

void convolute_stereo(AudioBuffer* audio, AudioBuffer* ir, AudioBuffer* dest);

Vector3 ray_get_pos(float t, const SoundRay* ray);
int ray_hit_sphere(const Sphere* sphere, const SoundRay* ray, float tmin, float tmax, SoundRayHit* hit);
int ray_hit_plane(const Plane* plane, const SoundRay* ray, float tmin, float tmax, SoundRayHit* hit);
Vector3 reflect_ray(Vector3 dir, Vector3 normal);
Vector3 sample_cosine_hemisphere(Vector3 normal);
Vector3 get_dir_fibonacci_sphere(int i, int n);
void emit_sound_rays(int n, Vector3 pos, const Room* room); // put audio sources in room
void record_impulse(Listener* listener, float distance, float* energy, int ear);
void record_thread_impulse(float(*l_ir)[MAX_IR_SAMPLES], float (*r_ir)[MAX_IR_SAMPLES], float distance, float* energy, int ear);


void camera_init(Camera* camera);

void room_init(Room* room, float width, float height, float depth);
void draw_room(Room* room);

void update_listener(Listener* listener);








