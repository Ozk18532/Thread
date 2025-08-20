#include <algorithm>   // para std::max_element
#include <chrono>      // para usar el tiempo en la semilla aleatoria
#include <cstdint>     // para usar enteros grandes (uint64_t)
#include <iostream>    // para imprimir en consola
#include <mutex>       // (se podría usar si quisiéramos imprimir dentro de los hilos)
#include <numeric>     // utilidades matemáticas (no usado mucho aquí)
#include <random>      // para generar números aleatorios
#include <string>
#include <thread>      // para trabajar con hilos
#include <utility>
#include <vector>      // para usar vectores (listas dinámicas)

// --------------------- Clase para generar números aleatorios ---------------------
class ThreadRNG {
public:
    // El constructor recibe un "hint" (pista) que se usa para hacer única la semilla
    explicit ThreadRNG(unsigned seed_hint)
    : eng_(make_seed(seed_hint)) {}

    // Devuelve un número entero aleatorio entre low y high
    int uniform_int(int low, int high) {
        std::uniform_int_distribution<int> dist(low, high);
        return dist(eng_);
    }

private:
    // Crea una semilla mezclando varias fuentes (random_device, tiempo, hint)
    static std::mt19937 make_seed(unsigned seed_hint) {
        std::random_device rd; // generador aleatorio del sistema
        auto now = static_cast<unsigned>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count()
        );
        std::seed_seq seq{ rd(), now, seed_hint, rd(), now ^ 0x9e3779b9U };
        return std::mt19937(seq);
    }

    std::mt19937 eng_; // motor de números aleatorios tipo Mersenne Twister
};

// --------------------- Clase que representa la tarea de un hilo ---------------------
class SumTask {
public:
    // Configuración para cada tarea
    struct Config {
        int id;              // Identificador del hilo (0, 1, 2, ...)
        int samples = 100;   // Cuántos números aleatorios sumar
        int min_val = 1;     // Valor mínimo posible
        int max_val = 1000;  // Valor máximo posible
    };

    // Constructor que guarda la configuración
    explicit SumTask(Config cfg)
    : cfg_(cfg), result_(0) {}

    // Esta función se ejecuta cuando lanzamos el hilo
    void operator()() {
        ThreadRNG rng(static_cast<unsigned>(cfg_.id)); // generador aleatorio para este hilo
        std::uint64_t acc = 0; // acumulador para la suma
        for (int i = 0; i < cfg_.samples; ++i) {
            acc += static_cast<std::uint64_t>(rng.uniform_int(cfg_.min_val, cfg_.max_val));
        }
        result_ = acc; // guardamos el resultado final
    }

    // Métodos para obtener los datos
    int id() const { return cfg_.id; }
    std::uint64_t result() const { return result_; }

private:
    Config cfg_;          // configuración de esta tarea
    std::uint64_t result_; // resultado de la suma
};

// --------------------- Clase que coordina todos los hilos ---------------------
class SumCoordinator {
public:
    // Constructor: crea las tareas necesarias
    SumCoordinator(int thread_count, int samples_per_thread, int minv, int maxv) {
        tasks_.reserve(thread_count);
        for (int i = 0; i < thread_count; ++i) {
            tasks_.emplace_back(SumTask::Config{ i, samples_per_thread, minv, maxv });
        }
    }

    // Lanza todos los hilos y espera a que terminen
    void run_all() {
        for (auto& task : tasks_) {
            threads_.emplace_back(std::ref(task)); // cada hilo ejecuta su tarea
        }
        for (auto& t : threads_) {
            if (t.joinable()) t.join(); // esperamos que todos terminen
        }
    }

    // Devuelve todos los resultados como pares (id, suma)
    std::vector<std::pair<int, std::uint64_t>> summaries() const {
        std::vector<std::pair<int, std::uint64_t>> out;
        out.reserve(tasks_.size());
        for (const auto& task : tasks_) {
            out.emplace_back(task.id(), task.result());
        }
        return out;
    }

    // Encuentra el hilo con el resultado más grande
    std::pair<int, std::uint64_t> best() const {
      auto it = std::max_element(
          tasks_.begin(), tasks_.end(),
          [](const SumTask& a, const SumTask& b){ return a.result() < b.result(); }
      );

        return { it->id(), it->result() };
    }

private:
    std::vector<SumTask> tasks_;     // lista de tareas
    std::vector<std::thread> threads_; // lista de hilos
};

// --------------------- Programa principal ---------------------
int main() {
    // Parámetros de la simulación
    constexpr int kThreads = 10;   // número de hilos
    constexpr int kSamples = 100;  // cuántos números por hilo
    constexpr int kMin = 1;        // mínimo
    constexpr int kMax = 1000;     // máximo

    // Creamos el coordinador con esas configuraciones
    SumCoordinator coord(kThreads, kSamples, kMin, kMax);
    coord.run_all(); // ejecutamos todos los hilos

    // Mostramos los resultados de cada hilo
    auto rows = coord.summaries();
    std::cout << "Resultados por hilo (suma de " << kSamples
              << " numeros entre " << kMin << " y " << kMax << "):\n";
    for (const auto& [id, total] : rows) {
        std::cout << "  Hilo #" << id << " -> total = " << total << '\n';
    }

    // Mostramos cuál fue el mejor
    auto [best_id, best_total] = coord.best();
    std::cout << "\nEl hilo con mayor puntaje es el #" << best_id
              << " con " << best_total << " puntos.\n";

    return 0;
}
