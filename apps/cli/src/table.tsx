import React, { useState, useEffect } from "react";
import { render, Text, Box } from "ink";

const users = [
  {
    id: 1,
    name: "Test",
    email: "email.test.1@gmail.com",
  },
  {
    id: 2,
    name: "Test 2",
    email: "email.test.2@gmail.com",
  },
  {
    id: 3,
    name: "Test 3",
    email: "email.test.3@gmail.com",
  },
];

const Table = () => (
  <Box flexDirection="column" width={40} borderStyle="single">
    <Box>
      <Box width="10%">
        <Text>ID</Text>
      </Box>

      <Box width="50%">
        <Text>Name</Text>
      </Box>

      <Box width="40%">
        <Text>Email</Text>
      </Box>
    </Box>

    {users.map((user) => (
      <Box key={user.id}>
        <Box width="10%">
          <Text>{user.id}</Text>
        </Box>

        <Box width="30%">
          <Text>{user.name}</Text>
        </Box>

        <Box width="60%">
          <Text>{user.email}</Text>
        </Box>
      </Box>
    ))}
  </Box>
);

const Counter = () => {
  const [counter, setCounter] = useState(0);

  useEffect(() => {
    const timer = setInterval(() => {
      setCounter((previousCounter) => previousCounter + 1);
    }, 100);

    return () => {
      clearInterval(timer);
    };
  }, []);

  return <Text color="green">{counter} tests passed</Text>;
};

render(
  <Box>
    <Table />
    <Counter />
  </Box>
);
