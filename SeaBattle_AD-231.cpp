#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <array>
#include <random>
#include <ctime>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <queue>

int CELL_SIZE = 30;
int GRID_SIZE = 10;
int PADDING = 50;
const int BOTTOM_PANEL = 110;
int WINDOW_WIDTH = GRID_SIZE * CELL_SIZE * 2 + PADDING * 3;
int WINDOW_HEIGHT = GRID_SIZE * CELL_SIZE + PADDING * 2 + BOTTOM_PANEL;

struct ThemeColors {
    sf::Color bgColor = sf::Color(30, 20, 10);
    sf::Color cellEmpty = sf::Color(40, 30, 20);
    sf::Color cellShip = sf::Color(80, 60, 40);
    sf::Color cellMiss = sf::Color(120, 100, 80);
    sf::Color cellHit = sf::Color(200, 60, 30);
    sf::Color cellSunk = sf::Color(30, 120, 200);
    sf::Color borderShot = sf::Color(255, 255, 0, 180);
    sf::Color highlightGood = sf::Color(60, 220, 60, 100);
    sf::Color highlightBad = sf::Color(220, 60, 60, 100);
    sf::Color textMain = sf::Color(220, 200, 160);
    sf::Color textAccent = sf::Color(220, 180, 60);
    sf::Color textSuccess = sf::Color(60, 220, 60);
    sf::Color miniCellSunk = sf::Color(30, 120, 200);
    sf::Color miniCellShot = sf::Color(220, 180, 60);
    sf::Color miniCellEmpty = sf::Color(60, 50, 40);
    sf::Color hint = sf::Color(220, 180, 60);
    sf::Color menuTitle = sf::Color(220, 180, 60);
    sf::Color menuInfo = sf::Color(120, 100, 80);
    sf::Color panelBg = sf::Color(20, 20, 20, 180);
};
ThemeColors theme; 

enum CellState { Empty, ShipCell, Miss, Hit };

struct Ship {
    int size;
    std::vector<sf::Vector2i> positions;
    int hits = 0;
    bool isSunk() const { return hits >= size; }
};

struct Effect {
    sf::CircleShape shape;
    float lifetime = 0.f;
    bool isWave = false;
    float wavePhase = 0.f;

    bool isExplosion = false;
    float explosionPhase = 0.f;
    sf::Vector2f explosionCenter;
};

struct Settings {
    int aiLevel = 1;
    int gridSize = 10;
    std::vector<int> shipSet = { 5,4,3,3,2 };
    int themeIdx = 0; 
    bool showHints = false; 
};

std::vector<ThemeColors> themes = {
    ThemeColors{},
    ThemeColors{
        sf::Color(220,220,220), sf::Color(200,200,200), sf::Color(120,180,220), sf::Color(180,180,180),
        sf::Color(220,80,80), sf::Color(80,180,220), sf::Color(255,200,0,180), sf::Color(60,220,60,100),
        sf::Color(220,60,60,100), sf::Color(40,40,40), sf::Color(120,120,120), sf::Color(60,220,60),
        sf::Color(80,180,220), sf::Color(255,200,0), sf::Color(200,200,200), sf::Color(255,200,0),
        sf::Color(80,180,220), sf::Color(120,120,120), sf::Color(200,200,200,180)
    }
};
class Board {
public:
    Board(bool revealShips, const Settings& settings)
        : showShips(revealShips), settings(settings), grid(settings.gridSize, std::vector<CellState>(settings.gridSize, Empty))
    {
        if (revealShips)
            ;
        else
            placeAllShips();
    }

    bool isAreaFree(int row, int col, int gridSize, const std::vector<std::vector<CellState>>& grid) const {
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                int nr = row + dr, nc = col + dc;
                if (nr >= 0 && nr < gridSize && nc >= 0 && nc < gridSize) {
                    if (grid[nr][nc] == ShipCell) return false;
                }
            }
        }
        return true;
    }

    void placeAllShips() {
        ships.clear();
        for (auto& row : grid)
            std::fill(row.begin(), row.end(), Empty);

        std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
        std::uniform_int_distribution<int> dirDist(0, 1);
        std::uniform_int_distribution<int> coordDist(0, settings.gridSize - 1);

        for (int sz : settings.shipSet) {
            bool placed = false;
            int attempts = 0;
            while (!placed && attempts < 1000) {
                int dir = dirDist(rng);
                int row = coordDist(rng);
                int col = coordDist(rng);
                std::vector<sf::Vector2i> pos;

                bool canPlace = true;
                for (int i = 0; i < sz; ++i) {
                    int r = row + (dir == 0 ? i : 0);
                    int c = col + (dir == 1 ? i : 0);
                    if (r < 0 || r >= settings.gridSize || c < 0 || c >= settings.gridSize ||
                        grid[r][c] != Empty || !isAreaFree(r, c, settings.gridSize, grid)) {
                        canPlace = false;
                        break;
                    }
                    pos.emplace_back(c, r);
                }

                if (canPlace && pos.size() == sz) {
                    Ship ship{ sz, pos };
                    for (auto& p : pos)
                        grid[p.y][p.x] = ShipCell;
                    ships.push_back(ship);
                    placed = true;
                }
                attempts++;
            }
        }
    }

    bool canPlaceShip(int x, int y, int size, bool vertical) const {
        for (int i = 0; i < size; ++i) {
            int nx = x + (vertical ? 0 : i);
            int ny = y + (vertical ? i : 0);
            if (nx < 0 || ny < 0 || nx >= settings.gridSize || ny >= settings.gridSize)
                return false;
            if (grid[ny][nx] != Empty || !isAreaFree(ny, nx, settings.gridSize, grid))
                return false;
        }
        return true;
    }

    void placeShipManual(int x, int y, int size, bool vertical) {
        std::vector<sf::Vector2i> pos;
        for (int i = 0; i < size; ++i) {
            int nx = x + (vertical ? 0 : i);
            int ny = y + (vertical ? i : 0);
            grid[ny][nx] = ShipCell;
            pos.emplace_back(nx, ny);
        }
        ships.push_back(Ship{ size, pos });
    }

    void clearShips() {
        for (auto& row : grid)
            std::fill(row.begin(), row.end(), Empty);
        ships.clear();
    }

    void setShips(const std::vector<Ship>& newShips, const std::vector<std::vector<CellState>>& newGrid) {
        ships = newShips;
        grid = newGrid;
    }

    bool receiveShot(const sf::Vector2i& cell) {
        if (cell.x < 0 || cell.x >= settings.gridSize || cell.y < 0 || cell.y >= settings.gridSize)
            return false;
        CellState& cs = grid[cell.y][cell.x];
        if (cs == Empty) {
            cs = Miss;
            return false;
        }
        if (cs == ShipCell) {
            cs = Hit;
            for (auto& ship : ships) {
                for (auto& p : ship.positions) {
                    if (p == cell) {
                        ship.hits++;
                        break;
                    }
                }
            }
            return true;
        }
        return false;
    }

    bool allSunk() const {
        for (const auto& ship : ships)
            if (ship.hits < ship.size)
                return false;
        return true;
    }

    bool isSunkCell(const sf::Vector2i& cell) const {
        for (const auto& ship : ships) {
            if (ship.isSunk()) {
                for (const auto& p : ship.positions) {
                    if (p == cell) return true;
                }
            }
        }
        return false;
    }

    bool isShotCell(int x, int y) const {
        return grid[y][x] == Miss || grid[y][x] == Hit;
    }

    bool isHitCell(int x, int y) const {
        return grid[y][x] == Hit;
    }

    bool isMissCell(int x, int y) const {
        return grid[y][x] == Miss;
    }

    bool isShipCell(int x, int y) const {
        return grid[y][x] == ShipCell;
    }

    int getSize() const { return settings.gridSize; }

    const std::vector<Ship>& getShips() const { return ships; }

    void draw(sf::RenderWindow& win, const sf::Vector2f& offset, std::vector<Effect>& effects, int highlightSize = 0, int highlightX = -1, int highlightY = -1, bool highlightVertical = false, bool highlightValid = false, bool showAllShips = false) const {
        sf::RectangleShape cellShape(sf::Vector2f(CELL_SIZE - 1, CELL_SIZE - 1));
        sf::RectangleShape border(sf::Vector2f(CELL_SIZE - 1, CELL_SIZE - 1));
        border.setFillColor(sf::Color::Transparent);
        border.setOutlineThickness(2);

        for (int r = 0; r < settings.gridSize; ++r) {
            for (int c = 0; c < settings.gridSize; ++c) {
                cellShape.setPosition(offset.x + c * CELL_SIZE, offset.y + r * CELL_SIZE);
                border.setPosition(cellShape.getPosition());

                sf::Vector2i cell(c, r);

                bool revealShip = showAllShips && isShipCell(c, r);

                switch (grid[r][c]) {
                case Empty:    cellShape.setFillColor(sf::Color(40, 30, 20)); break;
                case ShipCell: cellShape.setFillColor(showShips ? sf::Color(80, 60, 40) : sf::Color(40, 30, 20)); break;
                case Miss:     cellShape.setFillColor(sf::Color(120, 100, 80)); break;
                case Hit:      cellShape.setFillColor(sf::Color(200, 60, 30)); break;
                }

                if (isSunkCell(cell)) {
                    cellShape.setFillColor(sf::Color(30, 120, 200));
                    bool found = false;
                    for (const auto& e : effects) {
                        if (e.isWave && e.shape.getPosition() == cellShape.getPosition())
                            found = true;
                    }
                    if (!found) {
                        Effect wave;
                        float radius = CELL_SIZE / 2 - 2;
                        wave.shape = sf::CircleShape(radius);
                        wave.shape.setOrigin(radius, radius);
                        wave.shape.setPosition(cellShape.getPosition().x + CELL_SIZE / 2, cellShape.getPosition().y + CELL_SIZE / 2);
                        wave.lifetime = 1.5f;
                        wave.isWave = true;
                        wave.wavePhase = 0.f;
                        effects.push_back(wave);
                    }
                }



                win.draw(cellShape);

                if (isShotCell(c, r)) {
                    border.setOutlineColor(sf::Color(255, 255, 0, 180));
                    win.draw(border);
                }
            }
        }

        if (highlightSize > 0 && highlightX >= 0 && highlightY >= 0) {
            for (int i = 0; i < highlightSize; ++i) {
                int nx = highlightX + (highlightVertical ? 0 : i);
                int ny = highlightY + (highlightVertical ? i : 0);
                if (nx < 0 || ny < 0 || nx >= settings.gridSize || ny >= settings.gridSize)
                    continue;
                sf::RectangleShape hl(sf::Vector2f(CELL_SIZE - 1, CELL_SIZE - 1));
                hl.setPosition(offset.x + nx * CELL_SIZE, offset.y + ny * CELL_SIZE);
                hl.setFillColor(highlightValid ? sf::Color(60, 220, 60, 100) : sf::Color(220, 60, 60, 100));
                win.draw(hl);
            }
        }
    }

    std::vector<std::vector<CellState>> grid;

private:
    std::vector<Ship> ships;
    bool showShips;
    Settings settings;
};

class AI {
public:
    AI(const Settings& settings) : settings(settings), grid(settings.gridSize, std::vector<int>(settings.gridSize, 0)) {}

    void reset() {
        grid.assign(settings.gridSize, std::vector<int>(settings.gridSize, 0));
        huntMode = true;
        targets = std::queue<sf::Vector2i>();
        lastHit = sf::Vector2i(-1, -1);
        lastDir = -1;
        triedDirs.clear();
    }

    sf::Vector2i getMove(const Board& board, int aiLevel) {
        int N = board.getSize();
        if (aiLevel == 1) {
            std::vector<sf::Vector2i> candidates;
            for (int y = 0; y < N; ++y)
                for (int x = 0; x < N; ++x)
                    if (!board.isShotCell(x, y))
                        candidates.emplace_back(x, y);
            if (candidates.empty()) return sf::Vector2i(0, 0);
            std::uniform_int_distribution<int> dist(0, (int)candidates.size() - 1);
            return candidates[dist(rng)];
        }

        if (!targets.empty()) {
            sf::Vector2i t = targets.front();
            targets.pop();
            return t;
        }

        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                if (board.isHitCell(x, y)) {
                    for (auto d : dirs) {
                        int nx = x + d.x, ny = y + d.y;
                        if (nx >= 0 && nx < N && ny >= 0 && ny < N && !board.isShotCell(nx, ny)) {
                            targets.push(sf::Vector2i(nx, ny));
                        }
                    }
                }
            }
        }

        if (!targets.empty()) {
            sf::Vector2i t = targets.front();
            targets.pop();
            return t;
        }

        std::vector<sf::Vector2i> candidates;
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x)
                if (!board.isShotCell(x, y) && ((x + y) % 2 == 0))
                    candidates.emplace_back(x, y);
        if (candidates.empty()) {
            for (int y = 0; y < N; ++y)
                for (int x = 0; x < N; ++x)
                    if (!board.isShotCell(x, y))
                        candidates.emplace_back(x, y);
        }
        if (candidates.empty()) return sf::Vector2i(0, 0);
        std::uniform_int_distribution<int> dist(0, (int)candidates.size() - 1);
        return candidates[dist(rng)];
    }

private:
    Settings settings;
    std::vector<std::vector<int>> grid;
    std::queue<sf::Vector2i> targets;
    sf::Vector2i lastHit;
    int lastDir = -1;
    std::vector<int> triedDirs;
    bool huntMode = true;
    std::mt19937 rng{ static_cast<unsigned>(std::time(nullptr)) };
    const std::vector<sf::Vector2i> dirs = { {1,0},{-1,0},{0,1},{0,-1} };
};

class Game {
public:
    enum Screen { MENU, PLACING_CHOICE, PLACING, PLAYING, SETTINGS, EXIT };

    Game()
        : window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), L"Морський бій"),
        settings(), playerBoard(true, settings), aiBoard(false, settings), ai(settings), playerTurn(true), gameOver(false), screen(MENU), selected(0),
        shots(0), hits(0), misses(0), consecMiss(0), fullscreen(false)
    {
        window.setFramerateLimit(60);

        if (!font.loadFromFile("Resources/segoeuib.ttf") && !font.loadFromFile("Resources/segoeuib.ttf")) {
            std::cerr << "Помилка: не знайдено Resources/segoeui.ttf чи segoeuib.ttf" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        statusText.setFont(font);
        statusText.setCharacterSize(20);
        statusText.setFillColor(sf::Color(220, 200, 160));
        statusText.setPosition(PADDING, PADDING / 2);

        menuItems = { u8"Нова гра", u8"Налаштування", u8"Вихід" };
        for (size_t i = 0; i < menuItems.size(); ++i) {
            std::string str = menuItems[i];
            sf::Text t;
            t.setFont(font);
            t.setString(sf::String::fromUtf8(str.begin(), str.end()));
            t.setCharacterSize(32);
            t.setFillColor(sf::Color(220, 200, 160));
            t.setPosition(PADDING + 40, PADDING + 60 + i * 60);
            menuTexts.push_back(t);
        }
        updateSettingsText();

        placeText.setFont(font);
        placeText.setCharacterSize(24);
        placeText.setFillColor(sf::Color(220, 200, 160));
        placeText.setPosition(PADDING + 40, PADDING + 10);

        randomText.setFont(font);
        randomText.setCharacterSize(22);
        randomText.setFillColor(sf::Color(220, 180, 60));
        randomText.setString(sf::String::fromUtf8(u8"Випадково", u8"Випадково" + strlen(u8"Випадково")));
        randomText.setPosition(PADDING + 40, WINDOW_HEIGHT - BOTTOM_PANEL + 10);

        playText.setFont(font);
        playText.setCharacterSize(22);
        playText.setFillColor(sf::Color(60, 220, 60));
        playText.setString(sf::String::fromUtf8(u8"Грати", u8"Грати" + strlen(u8"Грати")));
        playText.setPosition(PADDING + 200, WINDOW_HEIGHT - BOTTOM_PANEL + 10);

        // Для выбора способа расстановки
        placeChoiceText1.setFont(font);
        placeChoiceText1.setCharacterSize(28);
        placeChoiceText1.setFillColor(sf::Color(220, 200, 160));
        placeChoiceText1.setString(sf::String::fromUtf8(u8"Виберіть спосіб розміщення кораблів:", u8"Виберіть спосіб розміщення кораблів:" + strlen(u8"Виберіть спосіб розміщення кораблів:")));
        placeChoiceText1.setPosition(PADDING + 40, PADDING + 40);

        placeChoiceText2.setFont(font);
        placeChoiceText2.setCharacterSize(26);
        placeChoiceText2.setFillColor(sf::Color(220, 180, 60));
        placeChoiceText2.setString(sf::String::fromUtf8(u8"1 — Випадково", u8"1 — Випадково" + strlen(u8"1 — Випадково")));
        placeChoiceText2.setPosition(PADDING + 40, PADDING + 120);

        placeChoiceText3.setFont(font);
        placeChoiceText3.setCharacterSize(26);
        placeChoiceText3.setFillColor(sf::Color(60, 220, 60));
        placeChoiceText3.setString(sf::String::fromUtf8(u8"2 — Вручну", u8"2 — Вручну" + strlen(u8"2 — Вручну")));
        placeChoiceText3.setPosition(PADDING + 40, PADDING + 170);
    }

    void recreateWindow(bool toFullscreen = false) {
        if (toFullscreen) {
            auto mode = sf::VideoMode::getDesktopMode();
            int maxCellW = (mode.width - PADDING * 3) / (settings.gridSize * 2);
            int maxCellH = (mode.height - PADDING * 2 - BOTTOM_PANEL) / settings.gridSize;
            CELL_SIZE = std::min(60, std::min(maxCellW, maxCellH));
            GRID_SIZE = settings.gridSize;
            WINDOW_WIDTH = GRID_SIZE * CELL_SIZE * 2 + PADDING * 3;
            WINDOW_HEIGHT = GRID_SIZE * CELL_SIZE + PADDING * 2 + BOTTOM_PANEL;
            window.create(mode, L"Морський бій", sf::Style::Fullscreen);
        }
        else {
            CELL_SIZE = 30;
            GRID_SIZE = settings.gridSize;
            WINDOW_WIDTH = GRID_SIZE * CELL_SIZE * 2 + PADDING * 3;
            WINDOW_HEIGHT = GRID_SIZE * CELL_SIZE + PADDING * 2 + BOTTOM_PANEL;
            window.create(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), L"Морський бій", sf::Style::Default);
        }
        window.setFramerateLimit(60);

        randomText.setPosition(PADDING + 40, WINDOW_HEIGHT - BOTTOM_PANEL + 10);
        playText.setPosition(PADDING + 200, WINDOW_HEIGHT - BOTTOM_PANEL + 10);
    }

    void run() {
        sf::Clock clock;
        while (window.isOpen()) {
            float dt = clock.restart().asSeconds();
            processEvents();
            updateEffects(dt);

            window.clear(sf::Color(30, 20, 10));

            switch (screen) {
            case MENU:
                drawMenu();
                break;
            case SETTINGS:
                drawSettings();
                break;
            case PLACING_CHOICE:
                drawPlacingChoice();
                break;
            case PLACING:
                drawPlacing();
                break;
            case PLAYING:
                playerBoard.draw(window, sf::Vector2f(PADDING, PADDING), effects);
                aiBoard.draw(window, sf::Vector2f(PADDING * 2 + settings.gridSize * CELL_SIZE, PADDING), effects);
                drawHighlight();
                drawEffects();
                drawStats();
                drawStatus();
                drawHint();
                drawMiniMap();
                break;
            case EXIT:
                window.close();
                break;
            }
            window.display();
        }
    }

private:
    sf::RenderWindow window;
    Settings settings;
    Board playerBoard, aiBoard;
    AI ai;
    bool playerTurn;
    bool gameOver;
    sf::Font font;
    sf::Text statusText;
    std::mt19937 rng{ static_cast<unsigned>(std::time(nullptr)) };
    std::vector<Effect> effects;
    sf::SoundBuffer hitBuffer, missBuffer;
    sf::Sound hitSound, missSound;
    sf::Music bgMusic;

    Screen screen;
    int selected;
    std::vector<std::string> menuItems;
    std::vector<sf::Text> menuTexts;
    sf::Text settingsText;

    int shots;
    int hits;
    int misses;
    int consecMiss;
    bool fullscreen;

    int settingsSelected = 0;
    std::vector<std::string> settingsOptions = {
        u8"Складність ШІ: ",
        u8"Розмір поля: ",
        u8"Набір кораблів: ",
        u8"Назад"
    };

    std::vector<int> shipsToPlace;
    int currentShipIdx = 0;
    bool placingVertical = false;
    sf::Text placeText, randomText, playText;

    sf::Text placeChoiceText1, placeChoiceText2, placeChoiceText3;

    void processEvents() {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::F11) {
                fullscreen = !fullscreen;
                recreateWindow(fullscreen);
            }

            if (screen == MENU) {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Up) {
                        selected = (selected + menuItems.size() - 1) % menuItems.size();
                    }
                    if (event.key.code == sf::Keyboard::Down) {
                        selected = (selected + 1) % menuItems.size();
                    }
                    if (event.key.code == sf::Keyboard::Enter || event.key.code == sf::Keyboard::Space) {
                        if (selected == 0) { screen = PLACING_CHOICE; }
                        else if (selected == 1) { screen = SETTINGS; }
                        else if (selected == 2) { screen = EXIT; }
                    }
                }
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    for (size_t i = 0; i < menuTexts.size(); ++i) {
                        if (menuTexts[i].getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y)) {
                            selected = static_cast<int>(i);
                            if (selected == 0) { screen = PLACING_CHOICE; }
                            else if (selected == 1) { screen = SETTINGS; }
                            else if (selected == 2) { screen = EXIT; }
                        }
                    }
                }
            }
            else if (screen == SETTINGS) {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Up) {
                        settingsSelected = (settingsSelected + settingsOptions.size() - 1) % settingsOptions.size();
                        updateSettingsText();
                    }
                    if (event.key.code == sf::Keyboard::Down) {
                        settingsSelected = (settingsSelected + 1) % settingsOptions.size();
                        updateSettingsText();
                    }
                    if (event.key.code == sf::Keyboard::Left) {
                        changeSetting(-1);
                        updateSettingsText();
                    }
                    if (event.key.code == sf::Keyboard::Right) {
                        changeSetting(1);
                        updateSettingsText();
                    }
                    if (event.key.code == sf::Keyboard::Enter || event.key.code == sf::Keyboard::Escape) {
                        if (settingsSelected == (int)settingsOptions.size() - 1 || event.key.code == sf::Keyboard::Escape) {
                            screen = MENU;
                        }
                    }
                }
            }
            else if (screen == PLACING_CHOICE) {
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::Num1 || event.key.code == sf::Keyboard::Numpad1) {
                        resetGame();
                        randomPlaceShips();
                        screen = PLAYING;
                    }
                    if (event.key.code == sf::Keyboard::Num2 || event.key.code == sf::Keyboard::Numpad2) {
                        resetGame();
                        screen = PLACING;
                    }
                    if (event.key.code == sf::Keyboard::Escape) {
                        screen = MENU;
                    }
                }
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    if (placeChoiceText2.getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y)) {
                        resetGame();
                        randomPlaceShips();
                        screen = PLAYING;
                    }
                    if (placeChoiceText3.getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y)) {
                        resetGame();
                        screen = PLACING;
                    }
                }
            }
            else if (screen == PLACING) {
                if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2i cell = getCellFromMouse(event.mouseButton.x, event.mouseButton.y, false);
                    if (cell.x != -1 && currentShipIdx < (int)shipsToPlace.size()) {
                        int size = shipsToPlace[currentShipIdx];
                        if (playerBoard.canPlaceShip(cell.x, cell.y, size, placingVertical)) {
                            playerBoard.placeShipManual(cell.x, cell.y, size, placingVertical);
                            currentShipIdx++;
                        }
                    }
                    if (randomText.getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y)) {
                        randomPlaceShips();
                    }
                    if (playText.getGlobalBounds().contains(event.mouseButton.x, event.mouseButton.y) && currentShipIdx >= (int)shipsToPlace.size()) {
                        screen = PLAYING;
                    }
                }
                if (event.type == sf::Event::KeyPressed) {
                    if (event.key.code == sf::Keyboard::R) {
                        placingVertical = !placingVertical;
                    }
                    if (event.key.code == sf::Keyboard::Escape) {
                        screen = MENU;
                    }
                }
            }
            else if (screen == PLAYING) {
                if (!gameOver && playerTurn &&
                    event.type == sf::Event::MouseButtonPressed &&
                    event.mouseButton.button == sf::Mouse::Left)
                {
                    handlePlayerClick(event.mouseButton.x, event.mouseButton.y);
                }
                if (gameOver && event.type == sf::Event::KeyPressed) {
                    screen = MENU;
                }
            }
        }
    }

    void changeSetting(int dir) {
        if (settingsSelected == 0) {
            settings.aiLevel += dir;
            if (settings.aiLevel < 1) settings.aiLevel = 2;
            if (settings.aiLevel > 2) settings.aiLevel = 1;
        }
        if (settingsSelected == 1) {
            settings.gridSize += dir;
            if (settings.gridSize < 6) settings.gridSize = 15;
            if (settings.gridSize > 15) settings.gridSize = 6;
        }
        if (settingsSelected == 2) {
            static std::vector<std::vector<int>> presets = {
                {5,4,3,3,2},
                {4,3,3,2,2,2},
                {3,3,2,2,2,1,1}
            };
            static int idx = 0;
            idx += dir;
            if (idx < 0) idx = (int)presets.size() - 1;
            if (idx >= (int)presets.size()) idx = 0;
            settings.shipSet = presets[idx];
        }
        recreateWindow(fullscreen);
    }

    void updateSettingsText() {
        std::ostringstream oss;
        oss << u8"Налаштування\n\n";
        for (size_t i = 0; i < settingsOptions.size(); ++i) {
            std::string line = settingsOptions[i];
            if (i == 0) {
                line += (settings.aiLevel == 1 ? u8"Простий" : u8"Розумний");
            }
            if (i == 1) {
                line += std::to_string(settings.gridSize) + "x" + std::to_string(settings.gridSize);
            }
            if (i == 2) {
                line += "[";
                for (size_t j = 0; j < settings.shipSet.size(); ++j) {
                    if (j) line += ",";
                    line += std::to_string(settings.shipSet[j]);
                }
                line += "]";
            }
            if (i == settingsSelected) line = "> " + line;
            oss << line << "\n";
        }
        oss << u8"\n←/→ — змінити, ↑/↓ — выбрати, Enter/Esc — назад\nF11 — повноекранний режим";
        std::string settingsStr = oss.str();
        settingsText.setFont(font);
        settingsText.setCharacterSize(24);
        settingsText.setFillColor(sf::Color(220, 200, 160));
        settingsText.setString(sf::String::fromUtf8(settingsStr.begin(), settingsStr.end()));
        settingsText.setPosition(PADDING + 40, PADDING + 40);
    }

    sf::Vector2i getCellFromMouse(int mouseX, int mouseY, bool aiBoardSide) {
        int offsetX = aiBoardSide ? (PADDING * 2 + settings.gridSize * CELL_SIZE) : PADDING;
        int offsetY = PADDING;
        int boardX = mouseX - offsetX;
        int boardY = mouseY - offsetY;
        if (boardX < 0 || boardY < 0) return sf::Vector2i(-1, -1);
        sf::Vector2i cell(boardX / CELL_SIZE, boardY / CELL_SIZE);
        if (cell.x < 0 || cell.x >= settings.gridSize || cell.y < 0 || cell.y >= settings.gridSize) return sf::Vector2i(-1, -1);
        return cell;
    }

    void handlePlayerClick(int x, int y) {
        sf::Vector2i cell = getCellFromMouse(x, y, true);
        if (cell.x == -1) return;
        bool hit = aiBoard.receiveShot(cell);
        shots++;
        if (hit) {
            this->hits++;
            consecMiss = 0;
        }
        else {
            misses++;
            consecMiss++;
        }
        if (hit) hitSound.play(); else missSound.play();
        addEffect(cell, hit);

        if (aiBoard.allSunk()) gameOver = true;
        else if (!hit) { playerTurn = false; aiMove(); }
    }

    void aiMove() {
        sf::Vector2i cell = ai.getMove(playerBoard, settings.aiLevel);
        bool hit = playerBoard.receiveShot(cell);
        addEffectAI(cell, hit);
        if (hit) hitSound.play(); else missSound.play();

        if (playerBoard.allSunk()) gameOver = true;
        else if (!hit) playerTurn = true;
        else aiMove();
    }

    void addEffect(sf::Vector2i cell, bool hit) {
        Effect e;
        float radius = CELL_SIZE / 2 - 2;
        e.shape = sf::CircleShape(radius);
        e.shape.setOrigin(radius, radius);
        sf::Vector2f base(PADDING * 2 + settings.gridSize * CELL_SIZE, PADDING);
        if (!hit) e.shape.setFillColor(sf::Color(180, 160, 120, 200));
        else      e.shape.setFillColor(sf::Color(200, 60, 30, 200));
        e.shape.setPosition(
            base.x + cell.x * CELL_SIZE + CELL_SIZE / 2,
            base.y + cell.y * CELL_SIZE + CELL_SIZE / 2
        );
        e.lifetime = 0.5f;
        e.isWave = false;
        effects.push_back(e);
    }

    void addEffectAI(sf::Vector2i cell, bool hit) {
        Effect e;
        float radius = CELL_SIZE / 2 - 2;
        e.shape = sf::CircleShape(radius);
        e.shape.setOrigin(radius, radius);
        sf::Vector2f base(PADDING, PADDING);
        if (!hit) e.shape.setFillColor(sf::Color(180, 160, 120, 200));
        else      e.shape.setFillColor(sf::Color(200, 60, 30, 200));
        e.shape.setPosition(
            base.x + cell.x * CELL_SIZE + CELL_SIZE / 2,
            base.y + cell.y * CELL_SIZE + CELL_SIZE / 2
        );
        e.lifetime = 0.5f;
        e.isWave = false;
        effects.push_back(e);
    }

    void updateEffects(float dt) {
        for (auto it = effects.begin(); it != effects.end();) {
            if (it->isWave) {
                it->wavePhase += dt * 2.5f;
                float scale = 1.0f + 0.2f * std::sin(it->wavePhase * 3.14f);
                it->shape.setScale(scale, scale);
                sf::Color col = it->shape.getFillColor();
                col.a = static_cast<sf::Uint8>(180 * std::max(0.f, 1.0f - it->wavePhase / 2.0f));
                it->shape.setFillColor(col);
                it->lifetime -= dt;
                if (it->lifetime <= 0.f) it = effects.erase(it);
                else ++it;
            }
            else {
                it->lifetime -= dt;
                float ratio = it->lifetime / 0.5f;
                it->shape.setScale(1.f + (1.f - ratio), 1.f + (1.f - ratio));
                sf::Color col = it->shape.getFillColor();
                col.a = static_cast<sf::Uint8>(255 * std::max(ratio, 0.f));
                it->shape.setFillColor(col);
                if (it->lifetime <= 0.f) it = effects.erase(it);
                else ++it;
            }
        }
    }

    void drawHighlight() {
        if (!playerTurn || gameOver) return;
        sf::Vector2i mouse = sf::Mouse::getPosition(window);
        sf::Vector2i cell = getCellFromMouse(mouse.x, mouse.y, true);
        if (cell.x == -1) return;
        sf::RectangleShape hl(sf::Vector2f(CELL_SIZE - 1, CELL_SIZE - 1));
        hl.setPosition(
            PADDING * 2 + settings.gridSize * CELL_SIZE + cell.x * CELL_SIZE,
            PADDING + cell.y * CELL_SIZE
        );
        hl.setFillColor(sf::Color(220, 180, 60, 80));
        window.draw(hl);
    }

    void drawEffects() {
        for (auto& e : effects)
            window.draw(e.shape);
    }

    void drawStatus() {
        std::string str;
        if (gameOver)
            str = u8"Гра закінчена! Натисніть будь-яку клавішу, щоб повернутися до меню.";
        else if (playerTurn)
            str = u8"Ваш хід: клацніть по ворожій сітці.";
        else
            str = u8"Хід противника...";
        statusText.setString(sf::String::fromUtf8(str.begin(), str.end()));
        window.draw(statusText);
    }

    void drawStats() {
        std::ostringstream oss;
        oss << u8"Пострілів: " << shots
            << u8"   Попадань: " << hits
            << u8"   Промахів: " << misses
            << u8"   Точність: ";
        if (shots > 0)
            oss << std::fixed << std::setprecision(1) << (100.0 * hits / shots) << "%";
        else
            oss << "-";
        std::string statsStr = oss.str();
        sf::Text stats(sf::String::fromUtf8(statsStr.begin(), statsStr.end()), font, 18);
        stats.setFillColor(sf::Color(220, 200, 160));
        stats.setPosition(PADDING, WINDOW_HEIGHT - BOTTOM_PANEL + 10);
        window.draw(stats);
    }

    void drawHint() {
        if (consecMiss >= 5 && !gameOver && playerTurn) {
            std::string hintStr = u8"Підказка: спробуйте стріляти \"шахівкою\" для пошуку кораблів!";
            sf::Text hint(sf::String::fromUtf8(hintStr.begin(), hintStr.end()), font, 18);
            hint.setFillColor(sf::Color(220, 180, 60));
            hint.setStyle(sf::Text::Bold);
            hint.setPosition(PADDING, WINDOW_HEIGHT - BOTTOM_PANEL + 40);
            window.draw(hint);
        }
    }

    void drawMenu() {
        std::string titleStr = u8"Морський бій";
        sf::Text title(sf::String::fromUtf8(titleStr.begin(), titleStr.end()), font, 40);
        title.setFillColor(sf::Color(220, 180, 60));
        title.setStyle(sf::Text::Bold);
        title.setPosition(PADDING + 30, PADDING);
        window.draw(title);

        for (size_t i = 0; i < menuTexts.size(); ++i) {
            if (static_cast<int>(i) == selected)
                menuTexts[i].setFillColor(sf::Color(220, 180, 60));
            else
                menuTexts[i].setFillColor(sf::Color(220, 200, 160));
            window.draw(menuTexts[i]);
        }
        std::string infoStr = u8"Навігація: стрілки ВВЕРХ/ВНИЗ, Enter — вибрати\nF11 — повноекранний режим";
        sf::Text info(sf::String::fromUtf8(infoStr.begin(), infoStr.end()), font, 16);
        info.setFillColor(sf::Color(120, 100, 80));
        info.setPosition(PADDING + 30, WINDOW_HEIGHT - BOTTOM_PANEL + 10);
        window.draw(info);
    }

    void drawSettings() {
        window.draw(settingsText);
    }

    void drawPlacingChoice() {
        window.draw(placeChoiceText1);
        window.draw(placeChoiceText2);
        window.draw(placeChoiceText3);
    }

    void drawPlacing() {
        sf::Vector2i mouse = sf::Mouse::getPosition(window);
        sf::Vector2i cell = getCellFromMouse(mouse.x, mouse.y, false);
        int size = (currentShipIdx < (int)shipsToPlace.size()) ? shipsToPlace[currentShipIdx] : 0;
        bool valid = false;
        if (cell.x != -1 && currentShipIdx < (int)shipsToPlace.size())
            valid = playerBoard.canPlaceShip(cell.x, cell.y, size, placingVertical);

        playerBoard.draw(window, sf::Vector2f(PADDING, PADDING), effects,
            size, cell.x, cell.y, placingVertical, valid);

        std::ostringstream oss;
        if (currentShipIdx < (int)shipsToPlace.size()) {
            oss << u8"Розставте корабель: " << shipsToPlace[currentShipIdx]
                << u8"  (R – повернути, ESC – у меню)";
        }
        else {
            oss << u8"Усі кораблі розставлені!";
        }
        std::string utf8String = oss.str();
        placeText.setString(sf::String::fromUtf8(utf8String.begin(), utf8String.end()));
        placeText.setPosition(PADDING + 10, PADDING - 40);
        window.draw(placeText);

        randomText.setPosition(PADDING + 10, PADDING + settings.gridSize * CELL_SIZE + 20);
        window.draw(randomText);
        if (currentShipIdx >= (int)shipsToPlace.size()) {
            playText.setPosition(PADDING + 200, PADDING + settings.gridSize * CELL_SIZE + 20);
            window.draw(playText);
        }
    }

    void drawMiniMap() {
        const float scale = 0.25f;
        const int miniCell = static_cast<int>(CELL_SIZE * scale);
        const int miniGrid = miniCell * settings.gridSize;
        const int margin = 10;
        sf::Vector2f pos(WINDOW_WIDTH - miniGrid * 2 - margin * 2, WINDOW_HEIGHT - miniGrid - margin - 10);

        sf::RectangleShape bg(sf::Vector2f(miniGrid * 2 + margin, miniGrid + margin));
        bg.setFillColor(sf::Color(20, 20, 20, 180));
        bg.setPosition(pos.x - margin, pos.y - margin);
        window.draw(bg);

        drawMiniBoard(playerBoard, pos, miniCell);
        drawMiniBoard(aiBoard, sf::Vector2f(pos.x + miniGrid + margin / 2, pos.y), miniCell);
    }

    void drawMiniBoard(const Board& board, const sf::Vector2f& offset, int cellSize) {
        for (int r = 0; r < settings.gridSize; ++r) {
            for (int c = 0; c < settings.gridSize; ++c) {
                sf::RectangleShape cellShape(sf::Vector2f(cellSize - 1, cellSize - 1));
                cellShape.setPosition(offset.x + c * cellSize, offset.y + r * cellSize);

                if (board.isSunkCell(sf::Vector2i(c, r)))
                    cellShape.setFillColor(sf::Color(30, 120, 200));
                else if (board.isShotCell(c, r))
                    cellShape.setFillColor(sf::Color(220, 180, 60));
                else
                    cellShape.setFillColor(sf::Color(60, 50, 40));

                window.draw(cellShape);
            }
        }
    }

    void resetGame() {
        playerBoard = Board(true, settings);
        aiBoard = Board(false, settings);
        ai.reset();
        playerTurn = true;
        gameOver = false;
        effects.clear();
        shots = 0;
        hits = 0;
        misses = 0;
        consecMiss = 0;

        shipsToPlace = settings.shipSet;
        currentShipIdx = 0;
        placingVertical = false;
        playerBoard.clearShips();
    }

    void randomPlaceShips() {
        playerBoard.clearShips();
        Board tmp(false, settings);
        playerBoard.setShips(tmp.getShips(), tmp.grid);
        currentShipIdx = (int)shipsToPlace.size();
    }
};

int main() {
    Game game;
    game.run();
    return 0;
}
