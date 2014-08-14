#include <string>

class Mic
{
private:
    bool        is_running;
    std::string file_name;
public:
    Mic();
    ~Mic();
    void record(std::string file_name);
    void play(std::string file_name);
    void pause();
    void stop();
};