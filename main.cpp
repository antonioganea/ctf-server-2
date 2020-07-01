#include <SFML/Graphics.hpp>
#include <SFML/Network.hpp>

#include <iostream>
#include <cstring>
#include <string>
#include <cmath>
#include <stdlib.h>
using namespace std;

#define SCORE_TO_CRACK 10000
bool PUZZLE_CRACKED = false;

void sendPositionOverSocket( sf::TcpSocket & sock, sf::Vector2i pos ){
    sock.setBlocking(true);

    char buf[8];

    memcpy(buf,&pos.x,4);
    memcpy(buf+4,&pos.y,4);

    sock.send(buf,8);

    cout << "sending bytes  ";
    for ( int i = 0; i < 8; i++ ){
        cout << (int)buf[i] << ' ';
    }

    sock.setBlocking(false);
}

float getVectorMagnitude(sf::Vector2f vec){
    return sqrt(vec.x*vec.x + vec.y*vec.y);
}

class CrackedText : public sf::Drawable{
private:
    sf::Font font;
    sf::Text text;
public:
    CrackedText(){
        font.loadFromFile("assets/bariol.otf");
        text.setFont(font);
        text.move(400,400);
        text.setString("CRACKED");
        text.setFillColor(sf::Color::Green);
        text.setCharacterSize(128);
    }

    void draw(sf::RenderTarget & t, sf::RenderStates states) const{
        t.draw(text);
    }
};

class ScoreText : public sf::Drawable{
private:
    sf::Font font;
    sf::Text text;
    int score;

    void internal_setScore(){
        string scoreText = "";
        scoreText += "Score : ";
        scoreText += std::to_string(score);
        text.setString(scoreText.c_str());
    }
public:
    ScoreText(){
        font.loadFromFile("assets/bariol.otf");
        text.setFont(font);
        text.move(1600,0);
        reset();
    }

    void reset(){
        score = 0;
        internal_setScore();
    }

    void add(int a){
        score += a;
        internal_setScore();
        if ( score > SCORE_TO_CRACK ){
            PUZZLE_CRACKED = true;
        }
    }

    void draw(sf::RenderTarget & t, sf::RenderStates states) const{
        t.draw(text);
    }
};

class Carrot : public sf::Drawable{
private:
    sf::Texture carrotTex;
    sf::Sprite carrotSpr;
    int gridX =-1 , gridY =-1;
public:
    Carrot(){
        carrotTex.loadFromFile("assets/carrot.png");
        carrotSpr.setTexture(carrotTex);
    }
    void setPosOnGrid(int x, int y){
        carrotSpr.setPosition(64*x,64*y);
        gridX = x;
        gridY = y;
    }
    sf::Vector2f getPosition(){
        return carrotSpr.getPosition();
    }
    void setRandomPos(int maxX, int maxY){
        int newx;
        int newy;
        do {
            newx = rand() % maxX;
            newy = rand() % maxY;
        } while ( gridX == newx && gridY == newy );

        setPosOnGrid(newx,newy);
    }
    void reset(){
        setRandomPos(20,15);
    }

    sf::Vector2i getGridPos(){
        return sf::Vector2i(gridX,gridY);
    }
    void draw(sf::RenderTarget & t, sf::RenderStates states) const{
        t.draw(carrotSpr);
    }
};

Carrot * CARROT = nullptr;
sf::TcpSocket * communicationSocket = nullptr;

class Bunny : public sf::Drawable{
private:
    sf::Texture bunnyTex;
    sf::Sprite spr;

    Carrot * target = nullptr;
    ScoreText * scoreText = nullptr;
    int gridX = 0;
    int gridY = 0;
public:
    Bunny(){
        bunnyTex.loadFromFile("assets/bunny.png");
        spr.setTexture(bunnyTex);
    }
    void draw(sf::RenderTarget & t, sf::RenderStates states) const{
        t.draw(spr);
    }
    int move(int x, int y){
        spr.move(x*64,y*64);

        gridX += x;
        gridY += y;

        if ( target ){
            sf::Vector2f distance = (target->getPosition() - spr.getPosition());
            float dist = getVectorMagnitude(distance);

            //cout << dist << endl;

            if ( dist < 5.f ){
                // explode the carrot or something
                target->reset();

                if ( scoreText ){
                    scoreText->add(1);
                }

                if ( communicationSocket )
                    sendPositionOverSocket(*communicationSocket,CARROT->getGridPos());
            }
        }
    }
    sf::Vector2i getGridPos(){
        return sf::Vector2i(gridX,gridY);
    }
    void reset(){
        spr.setPosition(0,0);
        gridX = 0;
        gridY = 0;

        if ( target ){
            sf::Vector2f distance = (target->getPosition() - spr.getPosition());
            float dist = getVectorMagnitude(distance);

            //cout << dist << endl;

            if ( dist < 5.f ){
                // explode the carrot or something
                target->reset();

                if ( scoreText ){
                    scoreText->add(1);
                }

                if ( communicationSocket )
                    sendPositionOverSocket(*communicationSocket,CARROT->getGridPos());
            }
        }
    }

    void setTarget( Carrot * _target ){
        target = _target;
    }
    void setScoreText( ScoreText * _target ){
        scoreText = _target;
    }
};



int main()
{
    sf::RenderWindow window(sf::VideoMode(1920, 1080), "CTF Challenge 2", sf::Style::Fullscreen);
    window.setFramerateLimit(30);

    srand(232513);

    Bunny bunny;
    Carrot carrot;
    CARROT = &carrot;
    bunny.setTarget(&carrot);
    carrot.setRandomPos(5,5);
    ScoreText scoreText;
    bunny.setScoreText(&scoreText);

    CrackedText crackedText;

    sf::Color backgroundColor(64,64,64);

        sf::Vector2u winSize = window.getSize();
        sf::Font bariol;
        bariol.loadFromFile("assets/bariol.otf");

        sf::Text iptext;
        iptext.setFont(bariol);
        iptext.setCharacterSize(32);

        int port = 3333;

        std::string iptextstr = sf::IpAddress::getLocalAddress().toString();
        iptextstr += ":";
        iptextstr += std::to_string(port);

        iptext.setString(iptextstr);

        sf::FloatRect ipbounds = iptext.getLocalBounds();
        iptext.setPosition(winSize.x/2-ipbounds.width/2,0);


    sf::TcpListener listener;
    listener.listen(port);
    listener.setBlocking(false);

    sf::TcpSocket client;
    client.setBlocking(false);

    sf::Packet inBuffer;

    bool connected = false;

    //cout << sf::Socket::Status::NotReady;

    char arrBuf[512];
    std::size_t received;

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
                window.close();
            if ( event.type == sf::Event::KeyPressed ){
                if (event.key.code == sf::Keyboard::W){
                    bunny.move(0,-1);
                }
                if (event.key.code == sf::Keyboard::S){
                    bunny.move(0,1);
                }
                if ( event.key.code == sf::Keyboard::D ){
                    bunny.move(1,0);
                }
                if ( event.key.code == sf::Keyboard::A ){
                    bunny.move(-1,0);
                }
                if ( event.key.code == sf::Keyboard::Escape ){
                    window.close();
                }
            }
        }

        if ( connected == false ){
            if ( listener.accept(client) == sf::Socket::Status::Done ){
                // client accepted
                connected = true;
                cout << "Socket connected!" << endl;
                communicationSocket = &client;
                bunny.reset();
                scoreText.reset();
                sendPositionOverSocket(client, CARROT->getGridPos());
                /*
                cout << "Waiting for something" << endl;
                client.setBlocking(true);
                //client.receive(inBuffer);
                client.receive(arrBuf,512,received);
                cout << "Received " << (int)received << " " << arrBuf << endl;
                client.setBlocking(false);
                */
            }
        }

        if ( connected ){
            sf::Socket::Status status = client.receive(arrBuf,512,received);
            if ( status != sf::Socket::Status::NotReady ){
                // cout << "Status : " << status << endl;
                for ( int i = 0; i < received; i++ ){
                    //cout << arrBuf[i];

                    switch(arrBuf[i]) {
                    case 0x0001:
                        bunny.move(0,-1);
                        break;
                    case 0x0002:
                        bunny.move(0,1);
                        break;
                    case 0x0003:
                        bunny.move(-1,0);
                        break;
                    case 0x0004:
                        bunny.move(1,0);
                        break;
                        /*
                    case 0x0005:
                        bunny.reset();
                        break;
                        */
                    }
                }
                //cout << status << " " << arrBuf << endl;
                //sf::Socket::Status::Partial;
            }

            if ( status == sf::Socket::Status::Disconnected ){
                connected = false;
                communicationSocket = nullptr;
                cout << "Socket disconnected" << endl;
            }
        }


        /*
        if ( client.receive(inBuffer) == sf::Socket::Status::Partial ){
            cout << "Received : " << inBuffer.getDataSize() << endl;
        }
        */

        window.clear(backgroundColor);
        window.draw(carrot);
        window.draw(bunny);
        window.draw(scoreText);
        window.draw(iptext);
        if ( PUZZLE_CRACKED )
            window.draw(crackedText);
        window.display();
    }

    return 0;
}
