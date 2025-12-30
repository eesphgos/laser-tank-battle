#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <windows.h> // برای رنگ‌ها در ویندوز
#include <algorithm>
#include <thread>
#include <chrono>
#include <queue>
#include <set>

using namespace std;

// کدهای رنگ برای کنسول ویندوز
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"
#define PURPLE "\033[35m"
#define PINK "\033[95m"

// جهت‌های ممکن برای آینه
enum MirrorDirection
{
    SLASH,
    BACKSLASH
}; // '/' و '\'

// ساختار آینه
struct Mirror
{
    MirrorDirection direction;
    int health; // 4 تا 1
    bool exists;

    Mirror() : direction(SLASH), health(4), exists(false) {}
};

// ساختار تانک
struct Tank
{
    int player; // 1 یا 2
    int x, y;
    bool alive;

    Tank(int p, int posX, int posY) : player(p), x(posX), y(posY), alive(true) {}
};

// ساختار سلول بازی
struct Cell
{
    bool hasLaserSource;
    int sourcePlayer;

    bool hasTank;
    int tankPlayer;
    int tankIndex;

    bool hasMirror;
    Mirror mirror; // فقط اگر hasMirror == true باشد معتبر است

    bool laserVisited;
    char laserPathChar;

    Cell() : hasLaserSource(false), sourcePlayer(0),
             hasTank(false), tankPlayer(0), tankIndex(-1),
             hasMirror(false), laserVisited(false), laserPathChar(' ') {}
};

// کلاس اصلی بازی
class LaserTankGame
{
private:
    int m, n;    // ابعاد صفحه
    Cell **grid; // ماتریس پویا
    vector<Tank> player1Tanks;
    vector<Tank> player2Tanks;
    int currentPlayer; // 1 یا 2
    int tanksPerPlayer;
    bool gameOver;
    int winner;
    chrono::steady_clock::time_point startTime;
    vector<string> logMessages;

public:
    LaserTankGame() : grid(nullptr), currentPlayer(1),
                      gameOver(false), winner(0)
    {
        srand(time(NULL));
        startTime = chrono::steady_clock::now();
    }

    ~LaserTankGame()
    {
        if (grid != nullptr)
        {
            for (int i = 0; i < m; i++)
            {
                delete[] grid[i];
            }
            delete[] grid;
        }
    }

    // تابع برای پاک کردن صفحه کنسول
    void clearScreen()
    {
        system("cls");
    }

    // دریافت ابعاد از کاربر
    void getDimensions()
    {
        cout << "=============================================\n";
        cout << "                    laser tank\n";
        cout << "=============================================\n\n";

        do
        {
            cout << "screen length (4 to 10):";
            cin >> m;
        } while (m < 4 || m > 10);

        do
        {
            cout << "screen width (4 to 10):";
            cin >> n;
        } while (n < 4 || n > 10);

        do
        {
            cout << "number of tank for each pleyer (minimum 1) :";
            cin >> tanksPerPlayer;
        } while (tanksPerPlayer < 1);

        // تخصیص حافظه پویا برای ماتریس
        grid = new Cell *[m];
        for (int i = 0; i < m; i++)
        {
            grid[i] = new Cell[n];
        }
    }

    // تولید نقشه با اعتبارسنجی
    void generateMap()
    {
        // 1. Place laser sources
        grid[0][0].hasLaserSource = true;
        grid[0][0].sourcePlayer = 1;

        grid[m - 1][n - 1].hasLaserSource = true;
        grid[m - 1][n - 1].sourcePlayer = 2;

        // 2. Generate mirrors (at least one mirror in each row)
        for (int i = 0; i < m; i++)
        {
            int mirrorsInRow = 0;
            int attempts = 0;

            // At least one mirror per row
            while (mirrorsInRow == 0 && attempts < 50)
            {
                for (int j = 0; j < n; j++)
                {
                    // Skip laser source cells
                    if (grid[i][j].hasLaserSource)
                        continue;

                    // Random chance to place mirror (30% for each cell)
                    if (rand() % 100 < 30)
                    {
                        if (!grid[i][j].hasMirror && !grid[i][j].hasTank)
                        {
                            grid[i][j].hasMirror = true;
                            grid[i][j].mirror.direction = (rand() % 2 == 0) ? SLASH : BACKSLASH;
                            grid[i][j].mirror.health = 4;
                            grid[i][j].mirror.exists = true;
                            mirrorsInRow++;
                        }
                    }
                }
                attempts++;
            }

            // If still no mirror in row, force place one
            if (mirrorsInRow == 0)
            {
                for (int j = 0; j < n; j++)
                {
                    if (!grid[i][j].hasLaserSource && !grid[i][j].hasTank)
                    {
                        grid[i][j].hasMirror = true;
                        grid[i][j].mirror.direction = (rand() % 2 == 0) ? SLASH : BACKSLASH;
                        grid[i][j].mirror.health = 4;
                        grid[i][j].mirror.exists = true;
                        break;
                    }
                }
            }
        }

        // 3. Validate map (no row/column completely blocked by mirrors)
        validateMap();

        // 4. Place tanks
        placeTanks();

        // 5. Validate safety zones
        validateSafetyZones();

        addLog("Game map generated successfully.");
    }

    // اعتبارسنجی نقشه
    void validateMap()
    {
        // بررسی سطرها
        for (int i = 0; i < m; i++)
        {
            int mirrorCount = 0;
            for (int j = 0; j < n; j++)
            {
                if (grid[i][j].mirror.exists)
                    mirrorCount++;
            }
            // اگر کل سطر آینه باشد، یکی را حذف کن
            if (mirrorCount == n)
            {
                for (int j = 0; j < n; j++)
                {
                    if (grid[i][j].mirror.exists && !grid[i][j].hasLaserSource)
                    {
                        grid[i][j].mirror.exists = false;
                        break;
                    }
                }
            }
        }

        // بررسی ستون‌ها
        for (int j = 0; j < n; j++)
        {
            int mirrorCount = 0;
            for (int i = 0; i < m; i++)
            {
                if (grid[i][j].mirror.exists)
                    mirrorCount++;
            }
            // اگر کل ستون آینه باشد، یکی را حذف کن
            if (mirrorCount == m)
            {
                for (int i = 0; i < m; i++)
                {
                    if (grid[i][j].mirror.exists && !grid[i][j].hasLaserSource)
                    {
                        grid[i][j].mirror.exists = false;
                        break;
                    }
                }
            }
        }
    }

    // قرار دادن تانک‌ها
    void placeTanks()
    {
        vector<pair<int, int>> availableCells;

        // Collect all available cells (no laser source, no mirror)
        for (int i = 0; i < m; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (!grid[i][j].hasLaserSource && !grid[i][j].hasMirror)
                {
                    availableCells.push_back({i, j});
                }
            }
        }

        // Shuffle available cells
        random_shuffle(availableCells.begin(), availableCells.end());

        // Place player 1 tanks
        int idx = 0;
        for (int i = 0; i < tanksPerPlayer; i++)
        {
            bool placed = false;
            while (idx < availableCells.size() && !placed)
            {
                int x = availableCells[idx].first;
                int y = availableCells[idx].second;

                // Check safety zone for player 2
                if (!isInSafetyZone(x, y, 2))
                {
                    grid[x][y].hasTank = true;
                    grid[x][y].tankPlayer = 1;
                    grid[x][y].tankIndex = i;

                    player1Tanks.push_back(Tank(1, x, y));
                    player1Tanks[i].alive = true;
                    placed = true;
                    idx++;
                }
                else
                {
                    idx++;
                }
            }
            if (!placed)
            {
                // Emergency placement if no suitable cell found
                for (int a = 0; a < m; a++)
                {
                    for (int b = 0; b < n; b++)
                    {
                        if (!grid[a][b].hasLaserSource && !grid[a][b].hasMirror && !grid[a][b].hasTank)
                        {
                            if (!isInSafetyZone(a, b, 2))
                            {
                                grid[a][b].hasTank = true;
                                grid[a][b].tankPlayer = 1;
                                grid[a][b].tankIndex = i;

                                player1Tanks.push_back(Tank(1, a, b));
                                player1Tanks[i].alive = true;
                                placed = true;
                                break;
                            }
                        }
                    }
                    if (placed)
                        break;
                }
            }
        }

        // Place player 2 tanks
        for (int i = 0; i < tanksPerPlayer; i++)
        {
            bool placed = false;
            while (idx < availableCells.size() && !placed)
            {
                int x = availableCells[idx].first;
                int y = availableCells[idx].second;

                // Check safety zone for player 1
                if (!isInSafetyZone(x, y, 1))
                {
                    grid[x][y].hasTank = true;
                    grid[x][y].tankPlayer = 2;
                    grid[x][y].tankIndex = i;

                    player2Tanks.push_back(Tank(2, x, y));
                    player2Tanks[i].alive = true;
                    placed = true;
                    idx++;
                }
                else
                {
                    idx++;
                }
            }
            if (!placed)
            {
                for (int a = 0; a < m; a++)
                {
                    for (int b = 0; b < n; b++)
                    {
                        if (!grid[a][b].hasLaserSource && !grid[a][b].hasMirror && !grid[a][b].hasTank)
                        {
                            if (!isInSafetyZone(a, b, 1))
                            {
                                grid[a][b].hasTank = true;
                                grid[a][b].tankPlayer = 2;
                                grid[a][b].tankIndex = i;

                                player2Tanks.push_back(Tank(2, a, b));
                                player2Tanks[i].alive = true;
                                placed = true;
                                break;
                            }
                        }
                    }
                    if (placed)
                        break;
                }
            }
        }
    }

    // بررسی محدوده امن
    bool isInSafetyZone(int x, int y, int player)
    {
        if (player == 1)
        {
            // محدوده امن بازیکن 1: (0,0) تا (2,2)
            return (x >= 0 && x <= 2 && y >= 0 && y <= 2);
        }
        else
        {
            // محدوده امن بازیکن 2: (m-3,n-3) تا (m-1,n-1)
            return (x >= m - 3 && x <= m - 1 && y >= n - 3 && y <= n - 1);
        }
    }

    // اعتبارسنجی محدوده امن
    void validateSafetyZones()
    {
        // بررسی تانک‌های بازیکن 2 در محدوده امن بازیکن 1
        for (const auto &tank : player2Tanks)
        {
            if (isInSafetyZone(tank.x, tank.y, 1))
            {
                // جابه‌جا کردن تانک
                moveTankToSafeZone(tank, 1);
            }
        }

        // بررسی تانک‌های بازیکن 1 در محدوده امن بازیکن 2
        for (const auto &tank : player1Tanks)
        {
            if (isInSafetyZone(tank.x, tank.y, 2))
            {
                // جابه‌جا کردن تانک
                moveTankToSafeZone(tank, 2);
            }
        }
    }

    // جابه‌جایی تانک به منطقه امن
    void moveTankToSafeZone(const Tank &tank, int enemyPlayer)
    {
        // پیدا کردن یک موقعیت جدید
        for (int i = 0; i < m; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (!grid[i][j].hasLaserSource && !grid[i][j].hasTank &&
                    !grid[i][j].mirror.exists)
                {
                    if (!isInSafetyZone(i, j, enemyPlayer))
                    {
                        // جابه‌جایی تانک
                        grid[tank.x][tank.y].hasTank = false;
                        grid[i][j].hasTank = true;
                        grid[i][j].tankPlayer = tank.player;
                        grid[i][j].tankIndex = (tank.player == 1) ? tank.x * n + tank.y : tank.x * n + tank.y + tanksPerPlayer;

                        // به‌روزرسانی موقعیت تانک در لیست
                        if (tank.player == 1)
                        {
                            player1Tanks[tank.x * n + tank.y].x = i;
                            player1Tanks[tank.x * n + tank.y].y = j;
                        }
                        else
                        {
                            player2Tanks[tank.x * n + tank.y - tanksPerPlayer].x = i;
                            player2Tanks[tank.x * n + tank.y - tanksPerPlayer].y = j;
                        }
                        return;
                    }
                }
            }
        }
    }

    // نمایش رابط کاربری
    void displayUI()
    {
        clearScreen();

        // Game header
        cout << "===================================================\n";
        cout << "        Laser Tank Squad - Strategic Battle\n";
        cout << "===================================================\n\n";

        // Status information
        cout << "Current Player: " << (currentPlayer == 1 ? RED "Player 1" RESET : BLUE "Player 2" RESET) << endl;
        cout << "Remaining Tanks: ";
        cout << RED << getAliveTankCount(1) << RESET << " (P1) - ";
        cout << BLUE << getAliveTankCount(2) << RESET << " (P2)\n";

        // Elapsed time
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(now - startTime);
        int minutes = elapsed.count() / 60;
        int seconds = elapsed.count() % 60;
        cout << "Time Elapsed: " << minutes << ":" << (seconds < 10 ? "0" : "") << seconds << "\n\n";

        // Display game grid
        displayGrid();

        // Recent logs
        cout << "\n--- Game Log ---\n";
        int startIdx = max(0, (int)logMessages.size() - 5);
        for (int i = startIdx; i < logMessages.size(); i++)
        {
            cout << logMessages[i] << endl;
        }
        cout << "----------------\n";
    }

    // نمایش گرید بازی
    void displayGrid()
    {
        // Display column numbers
        cout << "    ";
        for (int j = 0; j < n; j++)
        {
            cout << " " << j << "  ";
        }
        cout << "\n    ";
        for (int j = 0; j < n; j++)
        {
            cout << "----";
        }
        cout << "-\n";

        // Display each row
        for (int i = 0; i < m; i++)
        {
            // Row number
            cout << i << " |";

            for (int j = 0; j < n; j++)
            {
                Cell &cell = grid[i][j];

                // Priority 1: Laser path
                if (cell.laserVisited)
                {
                    cout << PINK << " " << cell.laserPathChar << " " << RESET << "|";
                    continue;
                }

                // Priority 2: Laser source
                if (cell.hasLaserSource)
                {
                    if (cell.sourcePlayer == 1)
                    {
                        cout << RED << " S1" << RESET << "|";
                    }
                    else
                    {
                        cout << BLUE << " S2" << RESET << "|";
                    }
                    continue;
                }

                // Priority 3: Tank
                if (cell.hasTank)
                {
                    // Check if tank is alive
                    bool isAlive = false;
                    if (cell.tankPlayer == 1 && cell.tankIndex < player1Tanks.size())
                    {
                        isAlive = player1Tanks[cell.tankIndex].alive;
                    }
                    else if (cell.tankPlayer == 2 && (cell.tankIndex) < player2Tanks.size())
                    {
                        isAlive = player2Tanks[cell.tankIndex].alive;
                    }

                    if (isAlive)
                    {
                        if (cell.tankPlayer == 1)
                        {
                            cout << RED << " T1" << RESET << "|";
                        }
                        else
                        {
                            cout << BLUE << " T2" << RESET << "|";
                        }
                    }
                    else
                    {
                        cout << "   |";
                    }
                    continue;
                }

                // Priority 4: Mirror
                // Priority 4: Mirror
                if (cell.hasMirror && cell.mirror.exists)
                {
                    if (cell.mirror.health > 0)
                    {
                        string color;
                        switch (cell.mirror.health)
                        {
                        case 4:
                            color = PURPLE;
                            break;
                        case 3:
                            color = BLUE;
                            break;
                        case 2:
                            color = GREEN;
                            break;
                        case 1:
                            color = YELLOW;
                            break;
                        default:
                            color = RED;
                            break;
                        }

                        char mirrorChar = (cell.mirror.direction == SLASH) ? '/' : '\\';
                        cout << color << " " << mirrorChar << " " << RESET << "|";
                    }
                    else
                    {
                        // آینه شکسته شده - نمایش به صورت X
                        cout << RED << " X " << RESET << "|";
                    }
                    continue;
                }

                // Empty cell
                cout << " . |";
            }

            cout << "\n    ";
            for (int j = 0; j < n; j++)
            {
                cout << "----";
            }
            cout << "-\n";
        }
    }

    // افزودن لاگ
    void addLog(const string &message)
    {
        logMessages.push_back("[LOG]: " + message);
    }

    // گرفتن تعداد تانک‌های زنده
    int getAliveTankCount(int player)
    {
        if (player == 1)
        {
            int count = 0;
            for (const auto &tank : player1Tanks)
            {
                if (tank.alive)
                    count++;
            }
            return count;
        }
        else
        {
            int count = 0;
            for (const auto &tank : player2Tanks)
            {
                if (tank.alive)
                    count++;
            }
            return count;
        }
    }

    // شروع بازی
    void startGame()
    {
        getDimensions();
        generateMap();

        while (!gameOver)
        {
            playTurn();
            checkWinConditions();
            if (!gameOver)
            {
                switchPlayer();
            }
        }

        displayFinalResult();
    }

    // اجرای یک نوبت
    void playTurn()
    {
        displayUI();

        cout << "\n[GND]: (N)Move Tank, (R)Rotate Mirror, (S)Tank Shoot, (E)Exit: ";
        char choice;
        cin >> choice;

        switch (toupper(choice))
        {
        case 'N':
            moveTankAction();
            break;
        case 'R':
            rotateMirrorAction();
            break;
        case 'S':
            tankShootAction();
            break;
        case 'E':
            exitAction();
            return;
        default:
            addLog("Invalid input! Turn skipped.");
            return;
        }

        // اگر بازی تمام شده باشد ادامه نده
        if (gameOver)
            return;

        // شلیک لیزر (اجباری)
        shootLaserAction();

        // اگر بازی تمام شده باشد ادامه نده
        if (gameOver)
            return;

        // سیستم فرسودگی و بازتولید آینه‌ها
        updateMirrors();

        // پاک کردن مسیر لیزر برای نوبت بعد
        clearLaserPaths();
    }

    // عمل حرکت تانک
    void moveTankAction()
    {
        cout << "Enter tank coordinates (x y): ";
        int x, y;
        cin >> x >> y;

        if (x < 0 || x >= m || y < 0 || y >= n)
        {
            addLog("Coordinates out of bounds!");
            return;
        }

        if (!grid[x][y].hasTank || grid[x][y].tankPlayer != currentPlayer)
        {
            addLog("No friendly tank at these coordinates!");
            return;
        }

        cout << "Enter direction (1-8 for 8 directions around): ";
        int dir;
        cin >> dir;

        // Calculate new coordinates
        int newX = x, newY = y;
        int dx = 0, dy = 0;

        switch (dir)
        {
        case 1:
            dx = -1;
            dy = -1;
            break;
        case 2:
            dx = -1;
            dy = 0;
            break;
        case 3:
            dx = -1;
            dy = 1;
            break;
        case 4:
            dx = 0;
            dy = -1;
            break;
        case 5:
            dx = 0;
            dy = 1;
            break;
        case 6:
            dx = 1;
            dy = -1;
            break;
        case 7:
            dx = 1;
            dy = 0;
            break;
        case 8:
            dx = 1;
            dy = 1;
            break;
        default:
            addLog("Invalid direction!");
            return;
        }

        newX = x + dx;
        newY = y + dy;

        // Check boundaries
        if (newX < 0 || newX >= m || newY < 0 || newY >= n)
        {
            addLog("Move out of board bounds!");
            return;
        }

        // Check destination cell
        Cell &dest = grid[newX][newY];

        // Cannot move onto a mirror
        if (dest.hasMirror)
        {
            addLog("Cannot move onto a mirror!");
            return;
        }

        // Check for enemy laser source (WIN CONDITION)
        if (dest.hasLaserSource && dest.sourcePlayer != currentPlayer)
        {
            gameOver = true;
            winner = currentPlayer;
            addLog("Tank reached enemy laser source! Player " + to_string(currentPlayer) + " wins!");
            return;
        }

        // Check for own laser source
        if (dest.hasLaserSource && dest.sourcePlayer == currentPlayer)
        {
            addLog("Cannot move onto your own laser source!");
            return;
        }

        // Check for tank collision
        if (dest.hasTank)
        {
            // Both tanks destroyed
            destroyTank(x, y);
            destroyTank(newX, newY);
            addLog("Two tanks collided and were destroyed!");
            return;
        }

        // Move the tank
        moveTank(x, y, newX, newY);
        addLog("Player " + to_string(currentPlayer) + " moved tank to (" +
               to_string(newX) + "," + to_string(newY) + ").");
    }

    // حرکت تانک
    void moveTank(int oldX, int oldY, int newX, int newY)
    {
        // به‌روزرسانی گرید
        grid[newX][newY].hasTank = true;
        grid[newX][newY].tankPlayer = grid[oldX][oldY].tankPlayer;
        grid[newX][newY].tankIndex = grid[oldX][oldY].tankIndex;

        grid[oldX][oldY].hasTank = false;

        // به‌روزرسانی موقعیت تانک در لیست
        if (grid[newX][newY].tankPlayer == 1)
        {
            player1Tanks[grid[newX][newY].tankIndex].x = newX;
            player1Tanks[grid[newX][newY].tankIndex].y = newY;
        }
        else
        {
            player2Tanks[grid[newX][newY].tankIndex - tanksPerPlayer].x = newX;
            player2Tanks[grid[newX][newY].tankIndex - tanksPerPlayer].y = newY;
        }
    }

    // نابودی تانک
    // نابودی تانک
    void destroyTank(int x, int y)
    {
        if (!grid[x][y].hasTank)
            return;

        int player = grid[x][y].tankPlayer;
        int index = grid[x][y].tankIndex;

        if (player == 1 && index < player1Tanks.size())
        {
            player1Tanks[index].alive = false;
        }
        else if (player == 2 && index < player2Tanks.size())
        {
            player2Tanks[index].alive = false;
        }

        // ریست کردن اطلاعات سلول
        grid[x][y].hasTank = false;
        grid[x][y].tankPlayer = 0;
        grid[x][y].tankIndex = -1;

        addLog("tank of player " + to_string(player) + " destroyed.");
    }
    // عمل چرخش آینه
    void rotateMirrorAction()
    {
        cout << "mirror location (x y): ";
        int x, y;
        cin >> x >> y;

        if (x < 0 || x >= m || y < 0 || y >= n)
        {
            addLog("out of screen location!");
            return;
        }

        if (!grid[x][y].mirror.exists)
        {
            addLog("not exist mirror in this location!");
            return;
        }

        // چرخش ۹۰ درجه
        grid[x][y].mirror.direction =
            (grid[x][y].mirror.direction == SLASH) ? BACKSLASH : SLASH;

        addLog("player " + to_string(currentPlayer) +
               " turned mirror at (" + to_string(x) + "," + to_string(y) +
               ") .");
    }

    // عمل شلیک تانک
    void tankShootAction()
    {
        cout << "location of tank shooter (x y): ";
        int x, y;
        cin >> x >> y;

        if (x < 0 || x >= m || y < 0 || y >= n)
        {
            addLog("out of screen location!");
            return;
        }

        if (!grid[x][y].hasTank || grid[x][y].tankPlayer != currentPlayer)
        {
            addLog("your tank not in this location!");
            return;
        }

        cout << "shoot direction (1-8 for 8 direction): ";
        int dir;
        cin >> dir;

        // محاسبه مختصات هدف
        int targetX = x, targetY = y;
        switch (dir)
        {
        case 1:
            targetX--;
            targetY--;
            break;
        case 2:
            targetX--;
            break;
        case 3:
            targetX--;
            targetY++;
            break;
        case 4:
            targetY--;
            break;
        case 5:
            targetY++;
            break;
        case 6:
            targetX++;
            targetY--;
            break;
        case 7:
            targetX++;
            break;
        case 8:
            targetX++;
            targetY++;
            break;
        default:
            addLog("invalid direction!");
            return;
        }

        // بررسی محدوده
        if (targetX < 0 || targetX >= m || targetY < 0 || targetY >= n)
        {
            addLog("shoot is out of range!");
            return;
        }

        // بررسی هدف
        if (grid[targetX][targetY].hasTank)
        {
            // نابودی تانک حریف
            destroyTank(targetX, targetY);
            addLog("enemy tank destroyed!");
        }
        else if (grid[targetX][targetY].hasLaserSource &&
                 grid[targetX][targetY].sourcePlayer != currentPlayer)
        {
            // نابودی منبع لیزر حریف
            gameOver = true;
            winner = currentPlayer;
            addLog("laser source of enemy destroyed!game over.");
        }
        else
        {
            addLog("shoot take targert.");
        }
    }

    // عمل خروج
    void exitAction()
    {
        gameOver = true;
        // برنده بازیکنی است که تانک بیشتری دارد
        int p1Tanks = getAliveTankCount(1);
        int p2Tanks = getAliveTankCount(2);

        if (p1Tanks > p2Tanks)
        {
            winner = 1;
        }
        else if (p2Tanks > p1Tanks)
        {
            winner = 2;
        }
        else
        {
            winner = 0; // تساوی
        }

        addLog("player " + to_string(currentPlayer) + " left game.");
    }

    // عمل شلیک لیزر
    void shootLaserAction()
    {
        cout << "Enter laser direction (H)orizontal or (V)ertical: ";
        char direction;
        cin >> direction;
        direction = toupper(direction);

        // موقعیت شروع (منبع لیزر بازیکن فعلی)
        int startX, startY;
        if (currentPlayer == 1)
        {
            startX = 0;
            startY = 0;
        }
        else
        {
            startX = m - 1;
            startY = n - 1;
        }

        // علامت‌گذاری سلول منبع
        grid[startX][startY].laserVisited = true;
        grid[startX][startY].laserPathChar = 'S';

        // شلیک در جهت انتخاب شده
        if (direction == 'H')
        {
            // افقی: هم به راست و هم به چپ
            simulateLaser(startX, startY, 0, 1, 0);  // به راست
            simulateLaser(startX, startY, 0, -1, 0); // به چپ
        }
        else if (direction == 'V')
        {
            // عمودی: هم به بالا و هم به پایین
            simulateLaser(startX, startY, 1, 0, 0);  // به پایین
            simulateLaser(startX, startY, -1, 0, 0); // به بالا
        }
        else
        {
            addLog("Invalid direction! Use H or V.");
            return;
        }

        // نمایش لاگ
        addLog("Player " + to_string(currentPlayer) + " fired laser (" +
               string(1, direction) + ").");

        // نمایش گرید برای دیدن مسیر لیزر
        displayUI();

        // تأخیر کوتاه برای دیدن مسیر لیزر
        cout << "\nPress Enter to continue...";
        cin.ignore();
        cin.get();
    }

    // شبیه‌سازی حرکت بازگشتی لیزر
    // Add this helper function to check for loops
    // شبیه‌سازی حرکت بازگشتی لیزر
    // Add this helper function to check for loops
    bool hasLoop(int x, int y, int dx, int dy, int depth)
    {
        // Simple loop prevention: if laser goes too deep (more than m*n steps), stop
        return depth > m * n * 2;
    }

    // اصلاح تابع simulateLaser بدون set
    void simulateLaser(int x, int y, int dx, int dy, int depth)
    {
        // جلوگیری از حلقه بی‌نهایت - اگر عمق خیلی زیاد شد متوقف کن
        if (hasLoop(x, y, dx, dy, depth))
        {
            return;
        }

        // بررسی محدوده
        if (x < 0 || x >= m || y < 0 || y >= n)
        {
            return; // لیزر از صفحه خارج شد
        }

        // اگر این سلول قبلاً توسط لیزر بازدید شده (برای نمایش)
        // فقط اگر هنوز لیزر علامت‌گذاری نشده بود
        if (!grid[x][y].laserVisited)
        {
            grid[x][y].laserVisited = true;
            if (dx != 0 && dy != 0)
            {
                grid[x][y].laserPathChar = '+';
            }
            else if (dx != 0)
            {
                grid[x][y].laserPathChar = '|';
            }
            else
            {
                grid[x][y].laserPathChar = '-';
            }
        }

        // محاسبه سلول بعدی
        int nextX = x + dx;
        int nextY = y + dy;

        // بررسی محدوده برای سلول بعدی
        if (nextX < 0 || nextX >= m || nextY < 0 || nextY >= n)
        {
            return; // لیزر از صفحه خارج شد
        }

        Cell &nextCell = grid[nextX][nextY];

        // بررسی برخورد با تانک
        if (nextCell.hasTank)
        {
            // نابودی تانک
            destroyTank(nextX, nextY);
            // علامت‌گذاری برخورد
            nextCell.laserVisited = true;
            nextCell.laserPathChar = 'X';
            return; // لیزر متوقف می‌شود
        }

        // بررسی برخورد با منبع لیزر حریف
        if (nextCell.hasLaserSource && nextCell.sourcePlayer != currentPlayer)
        {
            gameOver = true;
            winner = currentPlayer;
            nextCell.laserVisited = true;
            nextCell.laserPathChar = '!';
            addLog("Laser hit enemy laser source! Game over!");
            return; // لیزر متوقف می‌شود
        }

        // بررسی برخورد با آینه
        if (nextCell.hasMirror)
        {
            // کاهش سلامت آینه
            nextCell.mirror.health--;

            // علامت‌گذاری آینه
            nextCell.laserVisited = true;
            nextCell.laserPathChar = '*';

            // اگر آینه هنوز سالم باشد (health > 0) انعکاس می‌دهد
            if (nextCell.mirror.health >= 0)
            {
                // تغییر جهت بر اساس نوع آینه
                int newDx, newDy;

                if (nextCell.mirror.direction == SLASH)
                { // '/'
                    // قانون بازتاب: (dx, dy) -> (-dy, -dx)
                    newDx = -dy;
                    newDy = -dx;
                }
                else
                { // '\'
                    // قانون بازتاب: (dx, dy) -> (dy, dx)
                    newDx = dy;
                    newDy = dx;
                }

                // ادامه لیزر از آینه با جهت جدید
                simulateLaser(nextX, nextY, newDx, newDy, depth + 1);
            }
            else
            {
                // آینه شکسته است، لیزر در همان جهت ادامه می‌یابد
                simulateLaser(nextX, nextY, dx, dy, depth + 1);
            }
            return;
        }

        // اگر سلول خالی است یا منبع لیزر خودی است، ادامه بده
        simulateLaser(nextX, nextY, dx, dy, depth + 1);
    }
    // پردازش اثرات لیزر
    void processLaserEffects()
    {
        // در اینجا اثرات لیزر قبلاً در تابع simulateLaser اعمال شده
        // این تابع برای سازگاری با ساختار کلی است
    }

    // به‌روزرسانی آینه‌ها (فرسودگی و بازتولید)
    // به‌روزرسانی آینه‌ها (فرسودگی و بازتولید)
    void updateMirrors()
    {
        vector<pair<int, int>> brokenMirrors;

        // پیدا کردن آینه‌های شکسته
        for (int i = 0; i < m; i++)
        {
            for (int j = 0; j < n; j++)
            {
                if (grid[i][j].hasMirror && grid[i][j].mirror.exists && grid[i][j].mirror.health <= 0)
                {
                    brokenMirrors.push_back({i, j});
                }
            }
        }

        // حذف آینه‌های شکسته و ایجاد آینه جدید
        for (auto &pos : brokenMirrors)
        {
            int x = pos.first, y = pos.second;

            // حذف آینه شکسته
            grid[x][y].hasMirror = false;
            grid[x][y].mirror.exists = false;
            grid[x][y].mirror.health = 0;

            // پیدا کردن یک خانه خالی تصادفی برای آینه جدید
            vector<pair<int, int>> emptyCells;
            for (int i = 0; i < m; i++)
            {
                for (int j = 0; j < n; j++)
                {
                    if (!grid[i][j].hasLaserSource && !grid[i][j].hasTank &&
                        !grid[i][j].hasMirror)
                    {
                        emptyCells.push_back({i, j});
                    }
                }
            }

            if (!emptyCells.empty())
            {
                int idx = rand() % emptyCells.size();
                int newX = emptyCells[idx].first;
                int newY = emptyCells[idx].second;

                // ایجاد آینه جدید
                grid[newX][newY].hasMirror = true;
                grid[newX][newY].mirror.exists = true;
                grid[newX][newY].mirror.direction = (rand() % 2 == 0) ? SLASH : BACKSLASH;
                grid[newX][newY].mirror.health = 4;

                addLog("new mirror spnwn at (" + to_string(newX) + "," +
                       to_string(newY) + ") .");
            }
        }
    }
    // پاک کردن مسیرهای لیزر
    void clearLaserPaths()
    {
        for (int i = 0; i < m; i++)
        {
            for (int j = 0; j < n; j++)
            {
                grid[i][j].laserVisited = false;
                grid[i][j].laserPathChar = ' ';
            }
        }
    }

    // بررسی شرایط پیروزی
    void checkWinConditions()
    {
        // 1. نابودی کامل تانک‌های یک بازیکن
        int p1Tanks = getAliveTankCount(1);
        int p2Tanks = getAliveTankCount(2);

        if (p1Tanks == 0 && p2Tanks > 0)
        {
            gameOver = true;
            winner = 2;
            addLog("all tank of player 1 destroyed!");
            return;
        }

        if (p2Tanks == 0 && p1Tanks > 0)
        {
            gameOver = true;
            winner = 1;
            addLog("all tank of player 2 destroyed!");
            return;
        }

        // 2. ورود تانک به خانه منبع لیزر حریف
        // (این در تابع حرکت تانک بررسی می‌شود)

        // 3. اصابت مستقیم لیزر به منبع لیزر حریف
        // (این در تابع simulateLaser بررسی می‌شود)

        // 4. خروج توافقی در تابع exitAction بررسی می‌شود
    }

    // تعویض بازیکن
    void switchPlayer()
    {
        currentPlayer = (currentPlayer == 1) ? 2 : 1;
    }

    // نمایش نتیجه نهایی
    void displayFinalResult()
    {
        clearScreen();
        cout << "=========================================\n";
        cout << "              final resault\n";
        cout << "=========================================\n\n";

        if (winner == 0)
        {
            cout << "game draw!\n";
        }
        else
        {
            cout << "🏆 winner : player " << winner << " 🏆\n";
        }

        cout << "\nremaining tank:\n";
        cout << RED << "player 1: " << getAliveTankCount(1) << RESET << endl;
        cout << BLUE << "player 2: " << getAliveTankCount(2) << RESET << endl;

        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::seconds>(now - startTime);
        cout << "\ntotal game time: " << elapsed.count() << " second\n";

        cout << "\nenter any key to exit...";
        cin.get();
        cin.get();
    }
};

// تابع اصلی
int main()
{
    // تنظیم کدگذاری فارسی برای کنسول ویندوز
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    LaserTankGame game;
    game.startGame();

    return 0;
}