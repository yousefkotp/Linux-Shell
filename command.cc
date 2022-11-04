
/*
 * CS354: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#include "command.h"

/*	Global Variables	*/
char *home_dir = getenv("HOME");
char *path_to_current_directory[128];
int next_dir = 0;

/*	Prototypes	*/
void catchSIGINT(int);
int changeCurrentDirectory(void);
void add_dir_to_path(char *);

SimpleCommand::SimpleCommand()
{
	// Creat available space for 5 arguments initially
	_numberOfAvailableArguments = 5;
	_numberOfArguments = 0;
	_arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

void SimpleCommand::insertArgument(char *argument)
{
	if (_numberOfAvailableArguments == _numberOfArguments + 1)
	{
		// Double the available space
		_numberOfAvailableArguments *= 2;
		_arguments = (char **)realloc(_arguments,
									  _numberOfAvailableArguments * sizeof(char *));
	}

	_arguments[_numberOfArguments] = argument;

	// Add NULL argument at the end
	_arguments[_numberOfArguments + 1] = NULL;

	_numberOfArguments++;
}

Command::Command()
{
	// Create available space for one simple command
	_numberOfAvailableSimpleCommands = 1;
	_simpleCommands = (SimpleCommand **)
		malloc(_numberOfSimpleCommands * sizeof(SimpleCommand *));

	_numberOfSimpleCommands = 0;
	_outFile = 0;
	_inputFile = 0;
	_errFile = 0;
	_background = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
	if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
	{
		_numberOfAvailableSimpleCommands *= 2;
		_simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
													_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
	}

	_simpleCommands[_numberOfSimpleCommands] = simpleCommand;
	_numberOfSimpleCommands++;
}

void Command::clear()
{
	for (int i = 0; i < _numberOfSimpleCommands; i++)
	{
		for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
		{
			free(_simpleCommands[i]->_arguments[j]);
		}

		free(_simpleCommands[i]->_arguments);
		free(_simpleCommands[i]);
	}

	if (_outFile)
	{
		free(_outFile);
	}

	if (_inputFile)
	{
		free(_inputFile);
	}

	if (_errFile)
	{
		free(_errFile);
	}

	_numberOfSimpleCommands = 0;
	_outFile = 0;
	_append = 0;
	_inputFile = 0;
	_errFile = 0;
	_background = 0;
}

void Command::print()
{
	printf("\n\n");
	printf("              COMMAND TABLE                \n");
	printf("\n");
	printf("  #   Simple Commands\n");
	printf("  --- ----------------------------------------------------------\n");

	for (int i = 0; i < _numberOfSimpleCommands; i++)
	{
		printf("  %-3d ", i);
		for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
		{
			printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
		}
		printf("\n");
	}

	printf("\n\n");
	printf("  Output       Input        Error        Background\n");
	printf("  ------------ ------------ ------------ ------------\n");
	printf("  %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
		   _inputFile ? _inputFile : "default", _errFile ? _errFile : "default",
		   _background ? "YES" : "NO");
	printf("\n\n");
}

void Command::execute()
{
	// Don't do anything if there are no simple commands
	if (_numberOfSimpleCommands == 0)
	{
		prompt();
		return;
	}

	// Print contents of Command data structure
	print();

	int defaultIn = dup(0);
	int defaultOut = dup(1);

	int ip, op, err;
	if (_errFile)
	{
		err = open(_errFile, O_WRONLY | O_CREAT, 0777);
		dup2(err, 2);
	}
	if (_inputFile)
	{
		ip = open(_inputFile, O_RDONLY, 0777);
	}
	if (_outFile)
	{
		if (!_append)
			op = open(_outFile, O_WRONLY | O_CREAT, 0777);
		else
			op = open(_outFile, O_WRONLY | O_APPEND, 0777);
	}

	int fd[2];
	pipe(fd);
	for (int i = 0; i < _numberOfSimpleCommands; i++)
	{
		if (strcmp(_simpleCommands[i]->_arguments[0], "cd") == 0)
		{
			if (changeCurrentDirectory() == -1)
				printf("Error occurred. Make sure the directory you entered is valid\n");
		}
		else
		{
			if (i == 0)
			{
				if (_inputFile)
				{
					dup2(ip, 0);
					close(ip);
				}
				else
					dup2(defaultIn, 0);
			}
			else
			{
				dup2(fd[0], 0);
				close(fd[0]);
			}
			if (i == _numberOfSimpleCommands - 1)
			{
				if (_outFile)
					dup2(op, 1);
				else
					dup2(defaultOut, 1);
			}
			else
			{
				dup2(fd[1], 1);
				close(fd[1]);
			}
			int pid = fork();
			if (!pid)
			{ // child
				execvp(_simpleCommands[i]->_arguments[0], &_simpleCommands[i]->_arguments[0]);
			}
			else
			{ // parent
				dup2(defaultIn, 0);
				dup2(defaultOut, 1);
				if (!_background)
					waitpid(pid, 0, 0);
			}
		}
	}
	// Clear to prepare for next command
	clear();

	// Print new prompt
	prompt();
}

// Shell implementation

void Command::prompt()
{
	signal(SIGINT, catchSIGINT);
	printf("myshell>");
	for (int i = 0; i < next_dir; i++)
		printf("%s>", path_to_current_directory[i]);
	printf(" ");
	fflush(stdout);
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int yyparse(void);

/// @brief  SIGINT handler
void catchSIGINT(int sig_num)
{
	signal(SIGINT, catchSIGINT);
	Command::_currentCommand.clear();
	printf("\r\033[0J"); // Erase myshell> ^C
	Command::_currentCommand.prompt();
	fflush(stdout);
}

/// Executes the "cd" command
/// @returns 0 on success, -1 on error
int changeCurrentDirectory()
{
	int returnValue;
	char *path = Command::_currentSimpleCommand->_arguments[1];

	if (path)
		returnValue = chdir(path);
	else
		returnValue = chdir(home_dir);

	if (returnValue == 0 || !path)
		add_dir_to_path(path);

	Command::_currentCommand.clear();
	return returnValue;
}

void add_dir_to_path(char *directory)
{
	if (directory == NULL)
		next_dir = 0;
	else if (strcmp(directory, "..") == 0)
		next_dir--;
	else
		path_to_current_directory[next_dir++] = strdup(directory);
}

int main()
{
	chdir(home_dir);
	Command::_currentCommand.prompt();
	yyparse();
	return 0;
}
