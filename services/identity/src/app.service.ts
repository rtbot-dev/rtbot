import { Injectable } from "@nestjs/common";
import { client } from "@rtbot/postgres";

@Injectable()
export class AppService {
  constructor() {
    client;
  }

  getHello(): string {
    return "Hello World!";
  }
}
