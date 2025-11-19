NAME := webserv
CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98 -pedantic -Iinclude
LDFLAGS :=
SRC := $(wildcard src/*.cpp)
OBJ := $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
