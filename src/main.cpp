#include <bits/stdc++.h>
#include <unistd.h>
using namespace std;

const double MUTATION_PROBABILITY = 1;
const int DEFAULT_NEURONS_COUNT = 50;

const int FIELD_WIDTH = 200;
const int FIELD_HEIGHT = 50;
const double ENERGY_EARN = 1.0;
const double ENERGY_AT_BOTTOM = 0.1;
const double ENERGY_PER_MOVE = 1.0;
const double ENERGY_PER_ATTACK = 5.0;
const double ENERGY_EAT = 0.7;
const double EGAIN_PRIORITY = 1.1;
const double ENERGY_TO_FORK = 200.0;
const double ENERGY_LOSS_PER_NEIGH = 0.3;
const double ENERGY_POPULATION_PENALTY = 1.0 / 300.0;
const double RADIATION_AT_TOP = 0.2;
const double RADIATION_AT_BOTTOM = 0.8;
const double FOODGRAD_TOP = 0.55;
const double FOODGRAD_BOTTOM = 1.0;
const double FOOD_ENERGY_EARN = 1.6;
const int FOOD_AVAIL_INIT = 10;
const int FOOD_REFILL_EACH = 2;
const int SEASON_LENGTH = 4000;
const double GLOBAL_STARVATION_LEVEL = 0.05;

class Serializable
{
public:
    virtual ~Serializable() = default;
    virtual string to_string() = 0;
};

default_random_engine random_engine;

double random_double(double low, double high)
{
    uniform_real_distribution<double> dist(low, high);
    return dist(random_engine);
}

template <typename T>
T mutate(T value, double how)
{
    static_assert(is_floating_point<T>::value);
    return clamp(value + random_double(-how, how), T(0.0), T(1.0));
}

template <typename T>
vector<T> mutate(vector<T> vec, double how)
{
    for (auto& i : vec) {
        if (random_double(0, 1) < MUTATION_PROBABILITY) {
            i = mutate(i, how);
        }
    }
    return vec;
}

template <typename T>
string to_string(const vector<T>& vec, string separator = ", ")
{
    stringstream ss;
    ss << "[";
    for (auto it = vec.begin(); it < vec.end(); ++it) {
        if (it != vec.begin()) {
            ss << separator;
        }
        if constexpr (is_class<T>::value) {
            ss << to_string(*it, ", ");
        } else {
            ss << *it;
        }
    }
    ss << "]";
    return ss.str();
}

vector<vector<int>> eats(FIELD_HEIGHT, vector<int>(FIELD_WIDTH, 0));
int food_avail = FOOD_AVAIL_INIT;

double get_sun_strength(int epoch)
{
    return (sin(static_cast<double>(epoch) * M_PI / SEASON_LENGTH) + 1 - GLOBAL_STARVATION_LEVEL);
}

double get_energy([[maybe_unused]] int x, int y, int epoch)
{
    double sun_energy = ENERGY_EARN *
                        ((1.0 + ENERGY_AT_BOTTOM) - static_cast<double>(y) / FIELD_HEIGHT) *
                        get_sun_strength(epoch);
    double k = static_cast<double>(y) / FIELD_HEIGHT;
    k = (k - FOODGRAD_TOP) / (FOODGRAD_BOTTOM - FOODGRAD_TOP);
    double food_energy = max(0.0, k * FOOD_ENERGY_EARN);
    /*
    if (eats[y][x] >= food_avail) {
        // Everything is already eaten :(
        food_energy = 0;
    } else {
        // Yum-yum-yum!!!
        ++eats[y][x];
    }
    */
    return sun_energy + food_energy;
}

double get_radiation_level(int y)
{
    double k = static_cast<double>(y) / FIELD_HEIGHT;
    return k * RADIATION_AT_BOTTOM + (1 - k) * RADIATION_AT_TOP;
}

template <typename T>
int max_index(const vector<T>& vec)
{
    int ans = 0;
    for (int i = 0; i < static_cast<int>(vec.size()); ++i) {
        if (vec[i] > vec[ans]) {
            ans = i;
        }
    }
    // cout << "max_index: " << ans << " / " << vec.size() << endl;
    // cin.get();
    return ans;
}

atomic<bool> paused(false);
atomic<int> us_sleep(0);

void input_watcher(int)
{
    system("stty echo");
    cout << "\x1b[1;0H\x1b[0mEnter the sleep time in µseconds: ";
    cout.flush();
    int _us_sleep;
    cin >> _us_sleep;
    system("stty -echo");
    us_sleep = _us_sleep;
}

class Brain : public Serializable
{
public:
    Brain(int _n)
    {
        n = _n;
        connections.resize(n, vector<double>(n, 0.5));
        memory_strength.resize(n, 0.5);
        data.resize(n, 0);
    }
    Brain(int _n, vector<vector<double>>&& _conn, vector<double>&& _ms, vector<double>&& _data)
    {
        n = _n;
        connections = _conn;
        memory_strength = _ms;
        data = _data;
    }

    virtual ~Brain() = default;

    const vector<double>& process(const vector<double>& _data)
    {
        assert(static_cast<int>(_data.size()) < n);
        copy(_data.begin(), _data.end(), data.begin());
        for (int i = n - 1; i >= 0; --i) {
            double sum = 0, wsum = 0;
            for (int j = 0; j < n; ++j) {
                sum += data[j] * connections[i][j];
                wsum += connections[i][j];
            }
            sum /= wsum;
            double q = memory_strength[i];
            data[i] = data[i] * q + sum * (1 - q);
        }
        return data;
    }

    Brain mutated_copy(double how)
    {
        vector<vector<double>> _connections(mutate(connections, how));
        vector<double> _ms(mutate(memory_strength, how));
        vector<double> _data(mutate(data, how));
        return Brain(n, move(_connections), move(_ms), move(_data));
    }

    virtual string to_string() override
    {
        stringstream ss;
        ss << "Brain {" << endl;
        ss << "    conn = " << ::to_string<vector<double>>(connections, ",\n            ") << endl;
        ss << "    mems = " << ::to_string<double>(memory_strength) << endl;
        ss << "    data = " << ::to_string<double>(data) << endl;
        ss << "}";
        return ss.str();
    }

    int n;
    vector<vector<double>> connections;
    vector<double> memory_strength;
    vector<double> data;
};

// A bunch of forward declarations
class Mob;

bool has_mob_at(int x, int y);
bool move_mob(int ox, int oy, int nx, int ny);
double attack_mob_at(const Mob& attacker, int x, int y);

int _g_epoch = 0;

// And here come actual definitions
class Mob : public Serializable
{
public:
    explicit Mob(Brain _brain = Brain(DEFAULT_NEURONS_COUNT))
        : brain(_brain)
    {}

    virtual ~Mob() = default;

    virtual string to_string() override
    {
        stringstream ss;
        ss << "Mob(e=" << energy << ", x=" << x << ", y=" << y << ", " << brain.to_string() << ")";
        return ss.str();
    }

    bool decide(vector<double> output)
    {
        const int PRODUCE = 0;

        const int MOVE_L = 1;
        const int MOVE_R = 2;
        const int MOVE_U = 3;
        const int MOVE_D = 4;

        const int ATTACK_L = 5;
        const int ATTACK_R = 6;
        const int ATTACK_U = 7;
        const int ATTACK_D = 8;

        output[0] *= EGAIN_PRIORITY;

        switch (max_index(output)) {
        case MOVE_L:
            move(-1, 0);
            break;
        case MOVE_R:
            move(1, 0);
            break;
        case MOVE_U:
            move(0, -1);
            break;
        case MOVE_D:
            move(0, 1);
            break;

        case ATTACK_L:
            attack(-1, 0);
            break;
        case ATTACK_R:
            attack(1, 0);
            break;
        case ATTACK_U:
            attack(0, -1);
            break;
        case ATTACK_D:
            attack(0, 1);
            break;

        case PRODUCE:
            produce();
            break;

        default:
            assert(false);
        }

        if (has_mob_at(x + 1, y)) {
            energy -= ENERGY_LOSS_PER_NEIGH;
        }
        if (has_mob_at(x + 1, y + 1)) {
            energy -= ENERGY_LOSS_PER_NEIGH;
        }
        if (has_mob_at(x, y + 1)) {
            energy -= ENERGY_LOSS_PER_NEIGH;
        }
        if (has_mob_at(x - 1, y + 1)) {
            energy -= ENERGY_LOSS_PER_NEIGH;
        }
        if (has_mob_at(x - 1, y)) {
            energy -= ENERGY_LOSS_PER_NEIGH;
        }
        if (has_mob_at(x - 1, y - 1)) {
            energy -= ENERGY_LOSS_PER_NEIGH;
        }
        if (has_mob_at(x, y - 1)) {
            energy -= ENERGY_LOSS_PER_NEIGH;
        }
        if (has_mob_at(x + 1, y - 1)) {
            energy -= ENERGY_LOSS_PER_NEIGH;
        }

        return check_energy();
    }

    void move(int dx, int dy)
    {
        energy -= ENERGY_PER_MOVE;
        int nx = (x + dx + FIELD_WIDTH) % FIELD_WIDTH;
        int ny = clamp(y + dy, 0, FIELD_HEIGHT - 1);
        if (move_mob(x, y, nx, ny)) {
            x = nx;
            y = ny;
        }
    }

    void attack(int dx, int dy)
    {
        energy -= ENERGY_PER_ATTACK;
        int nx = (x + dx + FIELD_WIDTH) % FIELD_WIDTH;
        int ny = clamp(y + dy, 0, FIELD_HEIGHT - 1);
        if (has_mob_at(nx, ny)) {
            energy += attack_mob_at(*this, nx, ny);
        }
    }

    void produce()
    {
        energy += get_energy(x, y, _g_epoch);
    }

    bool check_energy()
    {
        return energy > 0;
    }

    bool take_turn(const vector<double> environment)
    {
        if (!check_energy()) {
            return false;
        }
        const vector<double>& processed = brain.process(environment);
        vector<double> output(processed.rbegin(), processed.rbegin() + 9);
        return decide(output);
    }

    int x;
    int y;
    double energy;
    Brain brain;
};

namespace std
{
    template <>
    struct hash<pair <int, int>>
    {
        size_t operator()(const pair <int, int>& k) const
        {
            // Compute individual hash values for first, second and third
            // http://stackoverflow.com/a/1646913/126995
            size_t res = 17;
            res = res * 31 + hash<int>()(k.first);
            res = res * 31 + hash<int>()(k.second);
            return res;
        }
    };
}

unordered_map<pair<int, int>, Mob> field;

bool has_mob_at(int x, int y)
{
    return field.count({ x, y }) > 0;
}
double attack_mob_at(const Mob& attacker, int x, int y)
{
    //assert(has_mob_at(x, y));
    Mob& victim = field.at({ x, y });
    double max_eat = attacker.energy * ENERGY_EAT;
    if (victim.energy < max_eat) {
        double ret = victim.energy;
        victim.energy = 0;
        return ret;
    } else {
        victim.energy -= max_eat;
        return max_eat;
    }
}

vector<pair<int, int>> to_remove;
vector<pair<pair<int, int>, Mob>> to_insert;

bool move_mob(int ox, int oy, int nx, int ny)
{
    // cerr << "move_mob" << ox << ", " << oy << ", " << nx << ", " << ny << endl;
    // abort();
    //assert(has_mob_at(ox, oy));
    if (has_mob_at(nx, ny)) {
        return false;
    }
    Mob mob = field.at({ ox, oy });
    to_remove.push_back({ ox, oy });
    to_insert.push_back({ { nx, ny }, mob });
    return true;
}

int cnt = 1;

string sun_strength_str(int epoch)
{
    int len = 20;
    string s;
    double k = (get_sun_strength(epoch) + GLOBAL_STARVATION_LEVEL) / 2;
    // cout << setprecision(10) << k << endl;
    int slen = round(len * k);
    // cin.get();
    // cout << slen << endl;
    s += "\x1b[48;2;255;255;0m";
    for (int i = 0; i < slen; ++i) {
        s += " ";
    }
    s += "\x1b[48;2;180;180;180m";
    for (int i = slen; i < len; ++i) {
        s += " ";
    }
    s += "\x1b[0m";
    return s;
}

void print_field(int epoch, int count = -1)
{
    cout << "\x1b[0m\x1b[0;0H";
    cout << "Epoch: " << epoch << ", population = " << cnt
         << ", sun strength: " << sun_strength_str(epoch) << endl;
    cnt = 0;
    for (int i = 0; i < FIELD_HEIGHT; ++i) {
        for (int j = 0; j < FIELD_WIDTH; ++j) {
            if (has_mob_at(j, i)) {
                ++cnt;
                int k = clamp(
                        static_cast<int>(field.at({ j, i }).energy * (255.0 / ENERGY_TO_FORK)),
                        0,
                        255);
                cout << "\x1b[38;2;255;" << k << ";" << k << "m#";
            } else {
                cout << " ";
            }
        }
        cout << endl;
    }
    cout.flush();
}

bool in_field(int x, int y)
{
    return 0 <= x && x < FIELD_WIDTH && 0 <= y && y < FIELD_HEIGHT;
}

void enable_echo()
{
    system("stty echo");
}

int main()
{
    atexit(enable_echo);
    system("stty -echo");
    signal(SIGQUIT, input_watcher);
    // std::thread input_watcher_thread(input_watcher);
    srand(time(nullptr));
    random_engine.seed(random_device()() ^ time(nullptr));
    cout.sync_with_stdio(false);
    Mob root;
    root.energy = 20;
    root.x = FIELD_WIDTH / 2;
    root.y = FIELD_HEIGHT / 2;
    field.insert({ { FIELD_WIDTH / 2, FIELD_HEIGHT / 2 }, root });

    const int fps = 15;
    auto prev = std::chrono::high_resolution_clock::now();

    int epoch = 0;
    while (true) {
        _g_epoch = epoch;
        if ((epoch & 0xF) == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            double dur = std::chrono::duration <double>(now - prev).count();
            if (dur >= 1.0 / fps) {
                print_field(epoch);
                prev = now;
            }
        }

        for (auto& [pos, mob] : field) {
            // cout << "AA: " << pos.first << ", " << pos.second << endl;
            // cin.get();
            vector<double> env(13, 0);

            // Q: Why not just 'auto [x, y] = pos;'?
            // A: It crashes my editor (Atom)
            int x, y;
            tie(x, y) = pos;

            if (has_mob_at(x + 1, y)) {
                env[0] = 1;
            }
            if (has_mob_at(x + 1, y + 1)) {
                env[1] = 1;
            }
            if (has_mob_at(x, y + 1)) {
                env[2] = 1;
            }
            if (has_mob_at(x - 1, y + 1)) {
                env[3] = 1;
            }
            if (has_mob_at(x - 1, y)) {
                env[4] = 1;
            }
            if (has_mob_at(x - 1, y - 1)) {
                env[5] = 1;
            }
            if (has_mob_at(x, y - 1)) {
                env[6] = 1;
            }
            if (has_mob_at(x + 1, y - 1)) {
                env[7] = 1;
            }
            env[8] = static_cast<double>(mob.x) / FIELD_WIDTH;
            env[9] = static_cast<double>(mob.y) / FIELD_HEIGHT;
            env[10] = mob.energy / ENERGY_TO_FORK;
            env[11] = epoch % 16;
            env[12] = random_double(0.0, 1.0);
            mob.energy -= ENERGY_POPULATION_PENALTY * cnt;
            bool alive = mob.take_turn(env);
            if (!alive) {
                to_remove.push_back(pos);
            }
            if (mob.energy > ENERGY_TO_FORK) {
                vector<pair<int, int>> c;
                if (!has_mob_at(x + 1, y) && in_field(x + 1, y)) {
                    c.push_back({ x + 1, y });
                }
                if (!has_mob_at(x + 1, y + 1) && in_field(x + 1, y + 1)) {
                    c.push_back({ x + 1, y + 1 });
                }
                if (!has_mob_at(x, y + 1) && in_field(x, y + 1)) {
                    c.push_back({ x, y + 1 });
                }
                if (!has_mob_at(x - 1, y + 1) && in_field(x - 1, y + 1)) {
                    c.push_back({ x - 1, y + 1 });
                }
                if (!has_mob_at(x - 1, y) && in_field(x - 1, y)) {
                    c.push_back({ x - 1, y });
                }
                if (!has_mob_at(x - 1, y - 1) && in_field(x - 1, y - 1)) {
                    c.push_back({ x - 1, y - 1 });
                }
                if (!has_mob_at(x, y - 1) && in_field(x, y - 1)) {
                    c.push_back({ x, y - 1 });
                }
                if (!has_mob_at(x + 1, y - 1) && in_field(x + 1, y - 1)) {
                    c.push_back({ x + 1, y - 1 });
                }
                if (!c.empty()) {
                    int cx, cy;
                    tie(cx, cy) = c.at(rand() % c.size());
                    mob.energy /= 2;
                    Mob clone = Mob(mob.brain.mutated_copy(get_radiation_level(mob.y)));
                    clone.energy = mob.energy;
                    to_insert.push_back({ { cx, cy }, clone });
                } else {
                    mob.energy = -100;
                }
            }
            // if (epoch % 1000 == 0) {
            //     mob.brain = mob.brain.mutated_copy(); // :)
            // }
        }
        for (const auto& pos : to_remove) {
            if (has_mob_at(pos.first, pos.second)) {
                field.erase(pos);
            }
        }
        for (auto ins : to_insert) {
            // assert(!has_mob_at(ins.first.first, ins.first.second));
            ins.second.x = ins.first.first;
            ins.second.y = ins.first.second;
            field.insert(ins);
        }

        to_remove.clear();
        to_insert.clear();
        while (paused) {
            this_thread::yield();
        }
        int _us_sleep = us_sleep;
        if (_us_sleep > 0) {
            usleep(_us_sleep);
        }
        ++epoch;
    }
}
